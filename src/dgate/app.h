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

#ifndef DGATE_APP_H
#define DGATE_APP_H

#include "dgate/dgate.h"
#include "dgate/g2.h"
#include <ev++.h>
#include <forward_list>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <unordered_set>
namespace dgate {

struct client_connection {
	int fd;
	std::unique_ptr<ev::io> watcher;
};

struct tx_state {
	char serial_buffer[512];// used to decode D-PRS data
	char tx_msg[21];        // might as well null-terminate this
	dv::header header;
	sockaddr_storage from;
	uint32_t count;     // Total count of packets
	uint8_t seqno;      // D-star frame count (mod 21)
	uint32_t bit_errors;// Number of bit errors detected
	uint16_t tx_id;
	uint8_t miniheader;
	int serial_pointer;
	bool local;

	void reset();
};

class app;
struct module {
	// This is probably bad but I'm SO TIRED.
	app* parent;
	void operator()(ev::timer&, int);

	char name;
	tx_state state;
	std::shared_ptr<ev::timer> timeout;
	mutable std::atomic_flag tx_lock;
};

class app {
	friend module;

public:
	app(std::string cs, std::unordered_set<char> modules);

	void run();

private:
	void unbind_all();

	void g2_readable_v4(ev::io&, int);
	void g2_readable_v6(ev::io&, int);
	void g2_handle_packet(const g2_packet& p, size_t len, const sockaddr_storage& from);
	void g2_handle_header(const g2_packet& p, size_t len, const sockaddr_storage& from);
	void g2_handle_voice(const g2_packet& p, size_t len, const sockaddr_storage& from);

	void dgate_readable(ev::io&, int);
	void dgate_client_readable(ev::io&, int);

	void tx_timeout(char module);

	void handle_header(const dv::header& h, char module);
	void handle_voice(const dv::rf_frame& h, char module);
	void handle_voice_end(const dv::rf_frame& h, char module);

	void write_all_dgate(const packet& p, std::size_t len);

	ev::dynamic_loop loop_;

	std::string cs_;

	int g2_sock_v4_;
	int g2_sock_v6_;

	int dgate_sock_;

	std::forward_list<client_connection> dgate_conns_;

	ev::io ev_g2_readable_v4_;
	ev::io ev_g2_readable_v6_;

	ev::io ev_dgate_readable_;

	std::unordered_set<char> enabled_modules_;// Enabled modules on this GATE.
	std::unordered_map<char, std::unique_ptr<module>> modules_;
};

}// namespace dgate

#endif
