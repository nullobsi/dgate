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

#ifndef ITAP_APP_H
#define ITAP_APP_H

#include "dgate/client.h"
#include <atomic>
#include <queue>
#include <random>
namespace itap {

class app : public dgate::client {
public:
	app(const std::string& dgate_socket_path, const std::string& itap_tty_path, const std::string& cs, char module);

	void run() override;

protected:
	void do_setup() override;
	void do_cleanup() override;

	void dgate_handle_header(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice_end(const dgate::packet& p, size_t len) override;

private:
	void itap_readable(ev::io&, int);
	void itap_ping(ev::timer&, int);
	void itap_timeout(ev::timer&, int);

	void send_from_queue();

	inline ssize_t itap_read(void* buf, size_t len);
	inline void itap_reply(const void* buf, size_t len);

	std::string cs_;
	std::string itap_tty_path_;

	uint8_t msg_length_;
	uint8_t msg_ptr_;
	uint8_t buf[255];

	char module_;
	uint16_t tx_id_;
	std::atomic_flag tx_lock;

	int itap_sock_;
	ev::io ev_itap_readable_;
	ev::timer ev_itap_ping_;
	ev::timer ev_itap_timeout_;

	std::minstd_rand rand_gen_;
	std::uniform_int_distribution<uint16_t> rand_dist_;

	bool acked_;
	std::queue<dgate::packet> queue_in_;
};

}// namespace itap

#endif
