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

#ifndef IRCDDB_CLIENT_H
#define IRCDDB_CLIENT_H
#include "common/threaded_queue.h"
#include "ircddb/irc_msg.h"
#include <atomic>
#include <cstdint>
#include <ev++.h>
#include <memory>
#include <sstream>
#include <string>

namespace ircddb {

enum client_state {
	CLOSED,
	ERRORED,
	AUTH_PASS,
	AUTH_NICK,
	AUTH_USER,
	CONNECTED
};

class client {
public:
	client(const std::string& host, const uint_least16_t port, const std::string& pass, const std::string& nick, const std::string& user, const std::string& realname, std::shared_ptr<ev::async> ev_msg_in_callback);

	// Resolves the host and opens a connection socket, but doesn't
	// connect.
	int setup();

	// Begin connection, setup event handler.
	int connect();

	void set_host(const std::string& host);
	void set_port(const uint_least16_t& port);
	void set_pass(const std::string& pass);
	void set_nick(const std::string& nick);
	void set_user(const std::string& user);
	void set_name(const std::string& realname);

	void queue_msg(const irc_msg& msg);

	std::atomic<client_state> state;
	threaded_queue<irc_msg> queue_msg_in;

	void run();

	~client();

private:
	int send_msg(const std::string& msg);

	void readable(ev::io& io, int revents);
	void writable(ev::io& io, int revents);
	void timeout(ev::timer& timer, int revents);

	void msg_out(ev::async&, int revents);
	int msg_in(const irc_msg& msg);

	void cleanup();

	threaded_queue<irc_msg> queue_msg_out_;

	ev::dynamic_loop loop_;

	ev::io ev_sock_readable_;
	ev::io ev_sock_writable_;
	ev::timer ev_sock_timeout_;

	ev::async ev_msg_out_;
	std::shared_ptr<ev::async> ev_msg_in_callback_;

	std::string host_;
	uint_least16_t port_;
	std::string pass_;
	std::string nick_;
	std::string user_;
	std::string realname_;
	int socketFd_;

	std::stringstream outBuffer_;
	std::stringstream inBuffer_;
};

}// namespace ircddb

#endif
