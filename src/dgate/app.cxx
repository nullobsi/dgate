#include "app.h"
#include "dgate/dgate.h"
#include "dgate/g2.h"
#include "dv/types.h"
#include <cerrno>
#include <cstring>
#include <ev++.h>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

namespace dgate {

void tx_state::reset()
{
	std::memset(this, 0, sizeof(*this));
	seqno = 20;
}

void module::operator()(ev::timer&, int)
{
	parent->tx_timeout(name);
};

app::app(std::string cs, std::unordered_set<char> modules, std::unique_ptr<ircddb::app> ircddb,
         std::shared_ptr<lmdb::env> env, std::shared_ptr<lmdb::dbi> cs_rptr,
         std::shared_ptr<lmdb::dbi> zone_ip4, std::shared_ptr<lmdb::dbi> zone_ip6,
         std::shared_ptr<lmdb::dbi> zone_nick)
	: loop_(), cs_(cs), g2_sock_v4_(-1), g2_sock_v6_(-1), dgate_sock_(-1),
	  ev_g2_readable_v4_(loop_), ev_g2_readable_v6_(loop_), ev_dgate_readable_(loop_),
	  enabled_modules_(modules), ircddb_(std::move(ircddb)), env_(env),
	  cs_rptr_(cs_rptr), zone_ip4_(zone_ip4), zone_ip6_(zone_ip6),
	  zone_nick_(zone_nick)
{
	for (auto m : enabled_modules_) {
		modules_[m] = std::make_unique<module>(m, tx_state(), std::make_shared<ev::timer>(loop_), this);
		modules_[m]->timeout->set(1., 1.);// TODO: configurable
		modules_[m]->timeout->set(modules_[m].get());
		modules_[m]->tx_lock.clear();
	}
	ev_g2_readable_v4_.set<app, &app::g2_readable_v4>(this);
	ev_g2_readable_v6_.set<app, &app::g2_readable_v6>(this);
	ev_dgate_readable_.set<app, &app::dgate_readable>(this);
}

void app::unbind_all()
{
	if (g2_sock_v6_ != -1) {
		close(g2_sock_v6_);
		g2_sock_v6_ = -1;
	}
	if (g2_sock_v4_ != -1) {
		close(g2_sock_v4_);
		g2_sock_v4_ = -1;
	}
	if (dgate_sock_ != -1) {
		close(dgate_sock_);
		dgate_sock_ = -1;
	}
}

void app::run()
{
	// TODO: add option to choose v4/v6, and listening IPs

	int error;
	struct addrinfo hints;
	struct addrinfo* servinfo = nullptr;

	// Listen to v6 socket
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(nullptr, "9011", &hints, &servinfo);
	if (error) {
		std::cerr << "gai error: " << gai_strerror(error) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		return;
	}

	g2_sock_v6_ = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	error = errno;
	freeaddrinfo(servinfo);

	if (g2_sock_v6_ == -1) {
		std::cerr << "dgate: socket(): could not create v6 socket: ";
		std::cerr << strerror(error) << std::endl;
		unbind_all();
		return;
	}

	// do NOT hybrid bind
	int sockopt = 1;
	setsockopt(g2_sock_v6_, IPPROTO_IPV6, IPV6_V6ONLY, &sockopt, sizeof(sockopt));

	// Listen to v4 socket
	hints.ai_family = AF_INET;
	error = getaddrinfo(nullptr, "40000", &hints, &servinfo);
	if (error) {
		std::cerr << "gai error: " << gai_strerror(error) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		unbind_all();
		return;
	}

	g2_sock_v4_ = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	error = errno;
	freeaddrinfo(servinfo);

	if (g2_sock_v4_ == -1) {
		std::cerr << "dgate: socket(): could not create v4 socket: ";
		std::cerr << strerror(error) << std::endl;
		unbind_all();
		return;
	}

	// Listen on UNIX socket
	dgate_sock_ = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	error = errno;

	if (dgate_sock_ == -1) {
		std::cerr << "dgate: socket(): could not create UNIX socket: ";
		std::cerr << strerror(error) << std::endl;
		unbind_all();
		return;
	}

	struct sockaddr_un name;
	std::memset(&name, 0, sizeof(sockaddr_un));
	name.sun_family = AF_UNIX;
	std::memcpy(name.sun_path, "dgate.sock", 11);

	error = bind(dgate_sock_, (sockaddr*)&name, sizeof(sockaddr_un));
	if (error) {
		error = errno;
		std::cerr << "dgate: bind(): failed: ";
		std::cerr << strerror(error) << std::endl;
		unbind_all();
		return;
	}

	error = listen(dgate_sock_, 5);
	if (error) {
		error = errno;
		std::cerr << "dgate: listen() failed: ";
		std::cerr << strerror(error) << std::endl;
		unbind_all();
		return;
	};

	// Set O_NONBLOCK
	fcntl(g2_sock_v6_, F_SETFL, O_NONBLOCK);
	fcntl(g2_sock_v4_, F_SETFL, O_NONBLOCK);
	fcntl(dgate_sock_, F_SETFL, O_NONBLOCK);

	// Setup event handlers
	ev_g2_readable_v6_.start(g2_sock_v6_, ev::READ);
	ev_g2_readable_v4_.start(g2_sock_v4_, ev::READ);
	ev_dgate_readable_.start(dgate_sock_, ev::READ);

	std::cout << "Entering loop..." << std::endl;

	loop_.run();
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
			std::cerr << "dgate: g2_readable_v4 called but read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dgate: g2_readable_v4: read() error: ";
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
			std::cerr << "dgate: g2_readable_v6 called but read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dgate: g2_readable_v6: read() error: ";
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

void app::g2_handle_header(const g2_packet& p, size_t len, const sockaddr_storage& from)
{
	char dst = p.header.destination_rptr_cs[7];
	if (!enabled_modules_.contains(dst)) return;

	if (!p.header.verify()) {
		std::cerr << "g2 header received with bad checksum!" << std::endl;
		return;
	}

	if (modules_[dst]->tx_lock.test_and_set()) {
		// TODO: some packets should be able to BREAK IN.
		// for the writer thread, probably just reset.
		std::cerr << "g2 header received but currently in transmission!" << std::endl;
		return;
	}

	modules_[dst]->timeout->again();

	// Reset info.
	modules_[dst]->state.reset();

	modules_[dst]->state.tx_id = p.streamid;
	modules_[dst]->state.from = from;

	handle_header(p.header, dst);
}

void app::handle_header(const dv::header& h, char m)
{
	modules_[m]->state.header = h;

	packet p;
	p.module = m;
	p.type = P_HEADER;
	p.header.id = modules_[m]->state.tx_id;
	p.header.h = h;

	write_all_dgate(p, packet_header_size);
}

void app::g2_handle_voice(const g2_packet& p, size_t len, const sockaddr_storage& from)
{
	auto id = p.streamid;
	auto seqno = p.ctrl & 0x1FU;// The MSBs are used for signaling

	char module = 0;
	for (const auto& m : modules_) {
		if (m.second->state.tx_id == id && m.second->tx_lock.test()) {
			module = m.first;
			break;
		}
	}

	if (module == 0) return;
	auto& mod = modules_[module];

	if (p.ctrl & 0x40U) {// END voice packet
		handle_voice_end(p.frame, module);
		mod->tx_lock.clear();
	}
	else {
		if (next_seqno(mod->state.seqno) != (p.ctrl & 0x1FU)) {
			std::cerr << "g2 " << id << ": packet received with wrong seqno?" << std::endl;
			// TODO: fill in blanks with silence? hold a small (1-2
			// frame) buffer to detect out of ordering?
			mod->state.seqno = prev_seqno(seqno);
		}
		mod->timeout->again();
		handle_voice(p.frame, module);
	}
	return;
}

void app::handle_voice(const dv::rf_frame& v, char m)
{
	auto& state = modules_[m]->state;
	state.count++;
	state.seqno = next_seqno(state.seqno);

	// TODO: dv fast data shouldn't be decoded
	auto f = v.decode();
	state.bit_errors += f.bit_errors;

	// Sequence 0 packet should ALWAYS be a sync packet.
	if (f.is_sync() && state.seqno != 0) {
		std::cerr << "module " << m << " is not seqno 0 but sync data frame received" << std::endl;
		state.seqno = 0;
	}

	// Parse miniheader if needed.
	if (state.seqno % 2 == 1) {
		state.miniheader = f.data[0];
		// The MSB 4 bits determine the type of data.
		int i;
		switch (state.miniheader & 0xF0U) {
		case dv::F_DATA:
			if (state.serial_pointer >= 509) break;
			std::memcpy(&state.serial_buffer[state.serial_pointer], &f.data[1], 2);
			state.serial_pointer += std::min(state.miniheader & 0x0FU, 0x02U);
			break;
		case dv::F_TXMSG:
			i = (state.miniheader & 0x0FU) * 5;
			if (i < 0 || i > 15) break;
			std::memcpy(&state.tx_msg[i], &f.data[1], 2);
			break;
		case dv::F_HEADER:
			// TODO: reconstruct header, or at least check validity
			break;
		case dv::F_FASTDATA_1:
		case dv::F_FASTDATA_2:
			// TODO: determine this
			break;
		case dv::F_CSQL:
			// TODO: this is probably not needed to be parsed
			break;
		}
	}
	else if (state.seqno > 0 && state.seqno % 2 == 0) {
		int i;
		switch (state.miniheader & 0xF0U) {
		case dv::F_DATA:
			if (state.serial_pointer >= 508) break;
			std::memcpy(&state.serial_buffer[state.serial_pointer], &f.data[0], 3);
			i = state.miniheader & 0x0FU;
			state.serial_pointer += std::max(i - 2, 0);
			break;
		case dv::F_TXMSG:
			i = (state.miniheader & 0x0FU) * 5;
			if (i < 0 || i > 15) break;
			std::memcpy(&state.tx_msg[i + 2], &f.data[0], 3);
			break;
		case dv::F_HEADER:
			// TODO: reconstruct header, or at least check validity
			break;
		case dv::F_FASTDATA_1:
		case dv::F_FASTDATA_2:
			// TODO: determine this
			break;
		case dv::F_CSQL:
			// TODO: this is probably not needed to be parsed
			break;
		}
	}

	auto reconstructed = f.encode();
	packet p;

	p.type = P_VOICE;
	p.module = m;
	p.voice.count = state.count;
	p.voice.id = state.tx_id;
	p.voice.seqno = state.seqno;
	p.voice.f = reconstructed;

	write_all_dgate(p, packet_voice_size);
}

void app::handle_voice_end(const dv::rf_frame& v, char m)
{
	auto& state = modules_[m]->state;
	state.count++;
	state.seqno = next_seqno(state.seqno);

	auto f = v.decode();
	state.bit_errors += f.bit_errors;

	packet p;
	p.module = m;
	p.type = P_VOICE_END;
	p.voice_end.count = state.count;
	p.voice_end.bit_errors = state.bit_errors;
	p.voice_end.id = state.tx_id;
	p.voice_end.f = f.encode();

	std::cout << "END TX: " << std::to_string(state.bit_errors) << " " << std::to_string(state.count) << " " << std::to_string(state.tx_id) << std::endl;
	std::cout.write(state.serial_buffer, state.serial_pointer);
	std::cout << std::endl;
	std::cout.write(state.header.own_cs, 8);
	std::cout << "/";
	std::cout.write(state.header.own_cs_ext, 4);
	std::cout << " -> ";
	std::cout.write(state.header.companion_cs, 8);
	std::cout << " via ";
	std::cout.write(state.header.destination_rptr_cs, 8);
	std::cout << ", ";
	std::cout.write(state.header.departure_rptr_cs, 8);
	std::cout << std::endl;

	write_all_dgate(p, packet_voice_end_size);
}

void app::dgate_readable(ev::io&, int)
{
	int client_fd = accept(dgate_sock_, nullptr, nullptr);

	if (client_fd == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dgate: dgate_readable called but accept() returned EAGAIN??" << std::endl;
		}
		else if (error == ECONNABORTED) {
			std::cerr << "dgate: connection waited too long in queue" << std::endl;
		}
		else {
			std::cerr << "dgate: accept() fail: ";
			std::cerr << strerror(error) << std::endl;
		}
		return;
	}

	std::cout << "Client connection accepted" << std::endl;

	client_connection conn;
	conn.fd = client_fd;
	conn.watcher = std::make_unique<ev::io>(loop_);
	conn.watcher->set<app, &app::dgate_client_readable>(this);
	conn.watcher->start(client_fd, ev::READ);

	dgate_conns_.push_back(std::move(conn));
}

void app::dgate_client_readable(ev::io& watcher, int)
{
	std::vector<client_connection>::size_type i;
	for (i = 0; i < dgate_conns_.size(); i++) {
		if (dgate_conns_[i].watcher.get() == &watcher) {
			dgate_client_handle_packet(i);
		}
	}
}

void app::dgate_client_handle_packet(int i)
{
	packet p;
	ssize_t count = read(dgate_conns_[i].fd, &p, sizeof(packet));
	if (count == 0) {
		// Client connection closed.
		close(dgate_conns_[i].fd);
		dgate_conns_.erase(dgate_conns_.begin() + i);

		return;
	}
	else if (count == -1) {
		int error = errno;
		if (error == EAGAIN || EWOULDBLOCK) {
			std::cerr << "dgate_client_handle_packet(): called but read() returned EAGAIN" << std::endl;

			return;
		}

		std::cerr << "dgate_client_handle_packet(): closing, read(): ";
		std::cerr << strerror(error) << std::endl;

		close(dgate_conns_[i].fd);
		dgate_conns_.erase(dgate_conns_.begin() + i);
		return;
	}

	if (!enabled_modules_.contains(p.module)) return;

	auto& mod = modules_[p.module];

	if (count == packet_header_size) {
		if (mod->tx_lock.test_and_set()) return;
		mod->state.reset();
		mod->state.tx_id = p.header.id;

		mod->timeout->again();
		handle_header(p.header.h, p.module);
	}
	else if (count == packet_voice_size) {
		if (mod->state.tx_id != p.voice.id) return;
		if (p.voice.seqno != next_seqno(mod->state.seqno)) {
			std::cerr << "dgate_client_handle_packet(): voice packet with wrong seqno received" << std::endl;
			// TODO: reconstruct?
			mod->state.seqno = prev_seqno(p.voice.seqno);
		}

		mod->timeout->again();
		handle_voice(p.voice.f, p.module);
	}
	else if (count == packet_voice_end_size) {
		if (mod->state.tx_id != p.voice.id) return;

		handle_voice_end(p.voice_end.f, p.module);
		mod->timeout->stop();
		mod->tx_lock.clear();
	} else {
		std::cout << " unknown" << std::endl;
	}

}

void app::write_all_dgate(const packet& p, std::size_t len)
{
	for (const auto& c : dgate_conns_) {
		ssize_t count = write(c.fd, &p, len);
		if (count == -1) {
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				// We really don't like this...
				// TODO: how prevalent? add send buffer?
				std::cerr << "write_all_dgate(): write() returned EAGAIN" << std::endl;
				continue;
			}
			std::cerr << "write_all_dgate(): closing, write(): ";
			std::cerr << strerror(error) << std::endl;
			continue;
		}
		if (count != (ssize_t)len) {
			// WTF?
			// TODO: should we quit the connection?
			std::cerr << "write_all_dgate(): WTF, datagram partial write?" << std::endl;
			continue;
		}
	}
}

void app::tx_timeout(char m)
{
	auto& mod = modules_[m];

	std::cerr << "timeout module " << m << std::endl;

	dv::rf_frame f;
	std::memcpy(&f.ambe, dv::rf_ambe_null, sizeof(f.ambe));
	std::memcpy(&f.data, dv::rf_data_end, sizeof(f.data));

	handle_voice_end(f, m);

	mod->timeout->stop();
	mod->tx_lock.clear();
}

}// namespace dgate
