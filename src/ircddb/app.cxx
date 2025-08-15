#include "app.h"
#include "ircddb/client.h"
#include <algorithm>
#include <cctype>
#include <future>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>

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

app::app(const std::string& cs, const std::vector<client_cfg>& configs, std::shared_ptr<lmdb::env> env, std::shared_ptr<lmdb::dbi> cs_rptr, std::shared_ptr<lmdb::dbi> zone_ip4, std::shared_ptr<lmdb::dbi> zone_ip6, std::shared_ptr<lmdb::dbi> zone_nick)
	: done(false), error(false), loop_(), ev_msg_out(loop_), env_(env), cs_rptr_(cs_rptr), zone_ip4_(zone_ip4), zone_ip6_(zone_ip6), zone_nick_(zone_nick)
{
	callsign_ = str_tolower(cs);
	// TODO: verify realname field
	std::string realname = ":CIRCDDB: dgate 0.0.1";

	for (const auto& c : configs) {
		auto store = std::make_shared<client_store>();

		store->watcher = std::make_shared<ev::async>(loop_);
		store->watcher->set<app, &app::msg_in>(this);
		store->watcher->start();

		store->current_nick = callsign_ + "-1";

		store->client = std::make_unique<client>(c.host, c.port, c.pass, store->current_nick, callsign_, "CIRCDDB:2.0.0 d-gate0001", store->watcher);
		store->cfg = c;

		clients_.push_back(std::move(store));
	}

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

void app::cleanup() {
	for (const auto& c : clients_) {
		c->watcher->stop();
	}
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
			for (std::vector<client>::size_type i = 0; i < clients_.size(); i++) {
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
}

static const std::regex SERVOPER_NICK_REGEX("@(s-[A-Za-z0-9\x5B-\x60\x7B-\x7D]+)", std::regex_constants::ECMAScript | std::regex_constants::optimize);

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

}// namespace ircddb
