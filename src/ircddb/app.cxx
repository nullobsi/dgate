//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
//
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.
//

#include "app.h"
#include "common/lmdb++.h"
#include "dgate/dgate.h"
#include "ircddb/client.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <future>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

std::string str_toupper(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
	return s;
}

std::string name_to_zone(const std::string& name)
{
	auto upper = str_toupper(name);
	upper.resize(7, ' ');
	return upper;
}

namespace ircddb {

app::app(const std::string& dgate_socket_path, const std::string& cs, std::unordered_set<char> enabled_mods, const std::vector<client_cfg>& configs, std::shared_ptr<lmdb::env> env, std::shared_ptr<lmdb::dbi> cs_rptr, std::shared_ptr<lmdb::dbi> zone_ip4, std::shared_ptr<lmdb::dbi> zone_ip6, std::shared_ptr<lmdb::dbi> zone_nick)
	: dgate::client(dgate_socket_path), done(false), error(false),
	  ev_msg_out(loop_), ev_g2_readable_v4_(loop_), ev_g2_readable_v6_(loop_), ev_dgate_readable_(loop_),
	  g2_sock_v4_(-1), g2_sock_v6_(-1), dgate_sock_(-1),
	  enabled_mods_(), env_(env), cs_rptr_(cs_rptr), zone_ip4_(zone_ip4), zone_ip6_(zone_ip6), zone_nick_(zone_nick)
{
	cs_short_lower_ = str_tolower(cs);

	// TODO: verify realname field
	std::string realname = ":CIRCDDB: dgate 0.0.1";

	for (const auto& m : enabled_mods) {
		enabled_mods_[m] = {{}, {}, false, 0, {' '}};
	}

	for (const auto& c : configs) {
		auto store = std::make_shared<client_store>();

		store->watcher = std::make_shared<ev::async>(loop_);
		store->watcher->set<app, &app::msg_in>(this);
		store->watcher->start();

		store->current_nick = cs_short_lower_ + "-1";

		store->client = std::make_unique<ircddb::client>(c.host, c.port, c.pass, store->current_nick, cs_short_lower_, "CIRCDDB:2.0.0 d-gate0001", store->watcher);
		store->cfg = c;

		clients_.push_back(std::move(store));
	}

	ev_g2_readable_v6_.set<app, &app::g2_readable_v6>(this);
	ev_g2_readable_v4_.set<app, &app::g2_readable_v4>(this);

	ev_msg_out.set<app, &app::msg_out>(this);
	ev_msg_out.start();
}

// This should run on a separate thread
void app::run()
{
	done = false;

	std::vector<std::future<void>> futures;

	for (const auto& c : clients_) {
		c->watcher->start();
		auto future = std::async(std::launch::async, [=]() {
			if (!c->client->connect())
				c->client->run();
		});
		futures.push_back(std::move(future));
	}

	loop_.run();
}

void app::do_cleanup()
{
	if (g2_sock_v6_ != -1) close(g2_sock_v6_);
	if (g2_sock_v4_ != -1) close(g2_sock_v4_);

	g2_sock_v6_ = -1;
	g2_sock_v4_ = -1;

	for (const auto& c : clients_) {
		c->watcher->stop();
	}
}

static inline int try_create_socket(const char* port, int family, int* fd)
{
	int error;
	struct addrinfo hints;
	struct addrinfo* servinfo = nullptr;

	// Listen to v6 socket
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(nullptr, port, &hints, &servinfo);
	if (error) {
		std::cerr << "gai error: " << gai_strerror(error) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		return -1;
	}

	*fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	error = errno;

	if (*fd == -1) {
		freeaddrinfo(servinfo);
		std::cerr << "dircddb: socket(): could not create socket: ";
		std::cerr << strerror(error) << std::endl;
		return -1;
	}

	if (family == AF_INET6) {
		// do NOT hybrid bind
		int sockopt = 1;
		setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY, &sockopt, sizeof(sockopt));
	}

	error = bind(*fd, servinfo->ai_addr, servinfo->ai_addrlen);
	if (error) {
		freeaddrinfo(servinfo);
		error = errno;
		std::cerr << "dircddb: bind(): could not bind: ";
		std::cerr << strerror(errno) << std::endl;
		return -1;
	}
	freeaddrinfo(servinfo);

	return 0;
}

void app::do_setup()
{
	// TODO: add option to choose v4/v6, and listening IPs

	int error;

	error = try_create_socket("9011", AF_INET6, &g2_sock_v6_);
	if (error) {
		cleanup();
		return;
	}

	error = try_create_socket("40000", AF_INET, &g2_sock_v4_);
	if (error) {
		cleanup();
		return;
	}

	fcntl(g2_sock_v6_, F_SETFL, O_NONBLOCK);
	fcntl(g2_sock_v4_, F_SETFL, O_NONBLOCK);

	ev_g2_readable_v6_.start(g2_sock_v6_, ev::READ);
	ev_g2_readable_v4_.start(g2_sock_v4_, ev::READ);
}

void app::queue_msg(const irc_msg& msg)
{
	queue_msg_out.push(msg);
	ev_msg_out.send();
}

void app::msg_out(ev::async&, int)
{
	// Use the prefix to determine which server to send to.
	// Normally we don't wanna send outgoing prefixes to the server.
	while (auto m = queue_msg_out.pop()) {
		auto msg = *m;
		if (msg.prefix) {
			int i;
			try {
				i = std::stoi(*msg.prefix);
			}
			catch (std::invalid_argument const&) {
				std::cerr << "BUG: invalid IRC message destination" << std::endl;
				return;// We really shouldn't be seeing this.
			}
			catch (std::out_of_range const&) {
				std::cerr << "BUG: invalid IRC message destination" << std::endl;
				return;
			}
			msg.prefix = {};

			if (0 <= i && i < (int)clients_.size()) {
				clients_[i]->client->queue_msg(msg);
			}
		}
		else {
			bool rewrite = msg.params && msg.params->list.size() > 0 && msg.params->list[0] == "s-";
			for (std::vector<client>::size_type i = 0; i < clients_.size(); i++) {
				if (rewrite) {
					msg.params->list[0] = clients_[i]->server_nick;
				}
				clients_[i]->client->queue_msg(msg);
			}
		}
		if (msg.command == "QUIT") {
			cleanup();
		}
	}
}

void app::msg_in(ev::async& watcher, int)
{
	for (std::vector<client_store>::size_type i = 0; i < clients_.size(); i++) {
		if (clients_[i]->watcher.get() == &watcher) {
			while (auto msg = clients_[i]->client->queue_msg_in.pop()) {
				handle_msg(i, *msg);
			}
		}
	}
}

void app::handle_msg(int i, const irc_msg& msg)
{
	//std::cout << msg;
	if (msg.code) {
		auto c = *msg.code;
		switch (c) {
		case irc::RPL_WELCOME:
			clients_[i]->client->queue_msg(irc_msg("JOIN", {}, clients_[i]->cfg.update_channel));
			break;
		case irc::RPL_NAMREPLY:
			handle_NAMREPLY(i, msg);
			break;
		case irc::RPL_WHOREPLY:
			handle_WHOREPLY(i, msg);
			break;
		}
	}
	else if (msg.command == "JOIN") {
		handle_JOIN(i, msg);
	}
	else if (msg.command == "QUIT") {
		handle_QUIT(i, msg);
	}
	else if (msg.command == "PRIVMSG") {
		// Many things to handle here!
		if (!msg.params) return;
		if (msg.params->list.size() < 1) return;// this is an invalid PRIVMSG

		if (msg.pfx->nick == clients_[i]->server_nick) {
			handle_server_msg(i, msg);
		}
		else if (msg.params->list[0] == clients_[i]->current_nick) {// this is a privmsg to us directly!
			handle_direct_msg(i, msg);
		}
	}
}

void app::handle_direct_msg(int i, const irc_msg& msg)
{
	if (!msg.params->trailer) return;// wtf?

	auto trailer = std::istringstream(*msg.params->trailer);

	std::string command;
	trailer >> command;

	if (command == "IDRT_PING") {
		// TODO: do we really need to do anything here?
		std::string from;
		trailer >> from;

		std::cout << "IDRT_PING from " << from << std::endl;
	}
}

void app::handle_server_msg(int i, const irc_msg& msg)
{
	if (!msg.params->trailer) return;

	auto text = std::istringstream(*msg.params->trailer);

	if (msg.params->list[0] == clients_[i]->current_nick) {// DM from server to us directly
		std::string one;
		text >> one;
		std::string two;
		text >> two;
		if (one == "IRCDDB" && two == "WATCHDOG:") {
			std::cout << "IRCDDB WATCHDOG responded" << std::endl;
			// TODO: watchdog timer
		}
	}
	else if (msg.params->list[0] == clients_[i]->cfg.update_channel) {
		// dates and times are in UTC
		// 2025-08-22 20:12:36 HEARD_CS RPT____B  (from: rpt-1)
		std::string date;
		text >> date;
		std::string time;
		text >> time;

		std::string my_cs;
		text >> my_cs;
		std::string rpt1;
		text >> rpt1;

		std::string nick;
		text >> nick;
		text >> nick;
		// trailing paren
		nick.resize(nick.size() - 1);

		// Just some sanity checking
		std::replace(my_cs.begin(), my_cs.end(), '_', ' ');
		my_cs = str_toupper(my_cs);
		my_cs.resize(8, ' ');

		std::replace(rpt1.begin(), rpt1.end(), '_', ' ');
		rpt1 = str_toupper(rpt1);
		rpt1.resize(8, ' ');

		auto t = std::time(nullptr);

		auto value = rpt1 + ' ' + std::to_string(t);

		{
			lmdb::txn wtxn = lmdb::txn::begin(*env_);
			cs_rptr_->put(wtxn, my_cs, value);
			wtxn.commit();
		}

		std::cout << "Heard " << my_cs << " on " << rpt1 << std::endl;
	}
}

static const std::regex SERVOPER_NICK_REGEX("@(s-[A-Za-z0-9\\x5B-\\x60\\x7B-\\x7D]+)", std::regex_constants::ECMAScript | std::regex_constants::optimize);

void app::handle_NAMREPLY(int i, const irc_msg& msg)
{
	// We might be able to get the "server user" here.
	std::smatch match;
	if (msg.params && msg.params->trailer && std::regex_search(*msg.params->trailer, match, SERVOPER_NICK_REGEX)) {
		clients_[i]->server_nick = match[1];
		std::cout << "server user recognized as " << clients_[i]->server_nick << std::endl;
	}
}

static const std::regex GATE_NICK_REGEX("^[A-Za-z0-9]+-[0-9]$", std::regex_constants::ECMAScript | std::regex_constants::optimize);

// TODO: if server user leaves and rejoins, or changes name, update the
// server user.
void app::handle_JOIN(int i, const irc_msg& msg)
{
	if (msg.params && *msg.params->trailer == clients_[i]->cfg.update_channel) {
		if (msg.pfx && msg.pfx->nick && *msg.pfx->nick == clients_[i]->current_nick) {
			std::cout << "Joined to update channel" << std::endl;

			get_all_gates(i);
		}
		else if (msg.pfx) {
			auto nick = msg.pfx->nick;
			auto host = msg.pfx->host;
			auto user = msg.pfx->user;
			if (nick && host && user && !nick->starts_with("u-") && std::regex_match(*nick, GATE_NICK_REGEX))
				update_gate(i, *nick, *user, *host);
		}
	}
}

void app::handle_WHOREPLY(int i, const irc_msg& msg)
{
	if (!msg.params) return;

	auto p = *msg.params;
	if (p.list.size() < 6) return;
	if (p.list[1] != clients_[i]->cfg.update_channel) return;
	if (std::regex_match(p.list[5], GATE_NICK_REGEX)) {
		update_gate(i, p.list[5], p.list[2], p.list[3]);
	}
	else if (p.list.size() > 6 && p.list[5].starts_with("s-") && p.list[6].ends_with('@')) {
		clients_[i]->server_nick = p.list[5];
		std::cout << "server user recognized as " << clients_[i]->server_nick << std::endl;
	}
}

// Send a WHO #dstar :* command to initialize mappings from GATE -> IP.
void app::get_all_gates(int i)
{
	irc_msg who("WHO", {clients_[i]->cfg.update_channel}, "*");
	clients_[i]->client->queue_msg(who);
}

// Update the memory-cache and insert a new GATE.
void app::update_gate(int i, const std::string& nick, const std::string& name, const std::string& host)
{
	auto now = std::time(nullptr);

	// Normalize to 7 character string
	auto zone = name_to_zone(name);

	auto wtxn = lmdb::txn::begin(*env_);

	zone_nick_->put(wtxn, std::to_string(i) + " " + zone, nick);
	if (clients_[i]->cfg.af == AF_INET) {
		zone_ip4_->put(wtxn, zone, std::to_string(i) + " " + std::to_string(now) + " " + host);
	}
	else {
		zone_ip6_->put(wtxn, zone, std::to_string(i) + " " + std::to_string(now) + " " + host);
	}

	wtxn.commit();
}

// Deletes a "zone/IRC server" -> "nick" mapping.
// We don't delete the IP entry because it could be from a different IRC
// server.
void app::delete_gate(int i, const std::string& name)
{
	auto zone = name_to_zone(name);

	auto wtxn = lmdb::txn::begin(*env_);
	zone_nick_->del(wtxn, std::to_string(i) + " " + zone);
	wtxn.commit();
}

void app::handle_QUIT(int i, const irc_msg& msg)
{
	if (msg.params && *msg.params->trailer == clients_[i]->cfg.update_channel && msg.pfx) {
		auto nick = msg.pfx->nick;
		auto host = msg.pfx->host;
		auto user = msg.pfx->user;
		if (nick && host && user && !nick->starts_with("u-") && std::regex_match(*nick, GATE_NICK_REGEX))
			delete_gate(i, *user);
	}
}

void app::g2_readable_v4(ev::io&, int)
{
	g2_packet p;
	sockaddr_storage from;
	socklen_t size = sizeof(sockaddr_storage);

	int count = recvfrom(g2_sock_v4_, (void*)&p, sizeof(g2_packet), 0, (sockaddr*)&from, &size);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dircddb: g2_readable_v4 called but read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dircddb: g2_readable_v4: read() error: ";
		std::cerr << strerror(error) << std::endl;
		// XXX SHOULD CLEANUP HERE
		// cleanup()
		return;
	}
	g2_handle_packet(p, count, from);
}

void app::g2_readable_v6(ev::io&, int)
{
	g2_packet p;
	sockaddr_storage from;
	socklen_t size = sizeof(sockaddr_storage);

	int count = recvfrom(g2_sock_v6_, (void*)&p, sizeof(g2_packet), 0, (sockaddr*)&from, &size);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dircddb: g2_readable_v6 called but read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dricddb: g2_readable_v6: read() error: ";
		std::cerr << strerror(error) << std::endl;
		// XXX SHOULD CLEANUP HERE
		// cleanup()
		return;
	}
	g2_handle_packet(p, count, from);
}

void app::g2_handle_packet(const g2_packet& p, size_t len, const sockaddr_storage& from)
{
	if (std::memcmp("DSVT", p.title, 4)) return;
	if (p.id != 0x20U) return;

	if (len == 56) g2_handle_header(p, len, from);
	if (len == 27) g2_handle_voice(p, len, from);
}

void app::g2_handle_header(const g2_packet& p, size_t, const sockaddr_storage& from)
{
	char dst = p.header.destination_rptr_cs[7];
	if (!enabled_mods_.contains(dst)) return;

	if (!p.header.verify()) {
		std::cerr << "g2 header received with bad checksum!" << std::endl;
		return;
	}

	if (enabled_mods_[dst].in_tx) return;

	enabled_mods_[dst].in_tx = true;
	enabled_mods_[dst].streamid = p.streamid;

	dgate::packet dp;
	dp.type = dgate::P_HEADER;
	dp.module = dst;
	dp.header.id = p.streamid;

	// TODO: do we need to re-write this?
	dp.header.h = p.header;

	dgate_reply(dp, dgate::packet_header_size);
}

void app::g2_handle_voice(const g2_packet& p, size_t, const sockaddr_storage&)
{
	auto id = p.streamid;
	auto seqno = p.ctrl & 0x1FU;// The MSBs are used for signaling

	auto mod = ' ';

	for (const auto& m : enabled_mods_) {
		if (m.second.streamid == id) {
			mod = m.first;
		}
	}

	if (mod == ' ') return;

	dgate::packet dp;

	if (p.ctrl & 0x40U) {// END voice packet
		dp.type = dgate::P_VOICE_END;
		dp.module = mod;
		dp.voice_end.f = p.frame;
		dp.voice_end.id = id;
		dp.voice_end.seqno = p.ctrl & 0x1FU;

		dgate_reply(dp, dgate::packet_voice_end_size);

		enabled_mods_[mod].in_tx = false;
		enabled_mods_[mod].streamid = 0;
	}
	else {
		dp.type = dgate::P_VOICE;
		dp.module = mod;
		dp.voice.id = id;
		dp.voice.f = p.frame;
		dp.voice.seqno = p.ctrl;

		dgate_reply(dp, dgate::packet_voice_size);
	}
	return;
}

void app::dgate_handle_header(const dgate::packet& p, size_t len)
{
	// Ignore non-local
	if (!(p.flags & dgate::P_LOCAL)) return;
	if (!enabled_mods_.contains(p.module)) return;

	auto& mod = enabled_mods_[p.module];
	mod.in_tx = true;
	mod.streamid = p.header.id;
	mod.tx_start = std::chrono::utc_clock::now();
	mod.h = p.header.h;
}

void app::dgate_handle_voice_end(const dgate::packet& p, size_t len)
{
	// Ignore non-local
	if (!(p.flags & dgate::P_LOCAL)) return;
	if (!enabled_mods_.contains(p.module)) return;

	auto& mod = enabled_mods_[p.module];
	if (mod.streamid != p.voice_end.id) return;

	// Now that we have the TX msg, we can send both updates.
	mod.in_tx = false;

	std::string my;
	my.assign(mod.h.own_cs, 8);
	std::replace(my.begin(), my.end(), ' ', '_');

	std::string rpt1;
	rpt1.assign(mod.h.departure_rptr_cs, 8);
	std::replace(rpt1.begin(), rpt1.end(), ' ', '_');

	std::string rpt2;
	rpt1.assign(mod.h.destination_rptr_cs, 8);
	std::replace(rpt2.begin(), rpt2.end(), ' ', '_');

	std::string ur;
	rpt1.assign(mod.h.companion_cs, 8);
	std::replace(ur.begin(), ur.end(), ' ', '_');

	std::string my_ext;
	my_ext.assign(mod.h.own_cs_ext, 4);
	std::replace(my_ext.begin(), my_ext.end(), ' ', '_');

	std::string dest;
	my_ext.assign(mod.dest, 8);
	std::replace(dest.begin(), dest.end(), ' ', '_');

	std::string tx_msg;
	tx_msg.assign(p.voice_end.tx_msg, 8);
	std::replace(tx_msg.begin(), tx_msg.end(), ' ', '_');

	auto time = std::chrono::utc_clock::now();

	uint8_t bit_errors = (((double)p.voice_end.bit_errors) / ((double)p.voice_end.count * 4)) * 100;

	std::string command = std::format("UPDATE {:%F %t} {} {} 0 {} {} {:02X} {:02X} {:02X} {} 00 {} {}", mod.tx_start, my, rpt1, rpt2, ur, mod.h.flags[0], mod.h.flags[1], mod.h.flags[2], my_ext, dest, tx_msg);

	std::string command2 = std::format("UPDATE {:%F %t} {} {} {} {} {:02X} {:02X} {:02X} {} # {:04x}00{:02x}____________", time, my, rpt1, rpt2, ur, mod.h.flags[0], mod.h.flags[1], mod.h.flags[2], my_ext, p.voice_end.count, bit_errors);

	queue_msg({"PRIVMSG", {"s-"}, command});
	queue_msg({"PRIVMSG", {"s-"}, command2});
}

}// namespace ircddb
