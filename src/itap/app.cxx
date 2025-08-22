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
#include "dgate/client.h"
#include "dgate/dgate.h"
#include "itap/itap.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <termios.h>
#include <thread>

#ifndef O_TTY_INIT
#define O_TTY_INIT 0
#endif

namespace itap {

app::app(const std::string& dgate_socket_path, const std::string& itap_tty_path, const std::string& cs, char module) : dgate::client(dgate_socket_path),  cs_(cs), itap_tty_path_(itap_tty_path), module_(module), itap_sock_(-1), ev_itap_readable_(loop_), ev_itap_ping_(loop_), ev_itap_timeout_(loop_), rand_gen_(getpid()), rand_dist_()
{
	cs_.resize(8, ' ');
	ev_itap_readable_.set<app, &app::itap_readable>(this);
	ev_itap_timeout_.set<app, &app::itap_timeout>(this);
	ev_itap_ping_.set<app, &app::itap_ping>(this);
}

void app::do_setup()
{
	itap_sock_ = open(itap_tty_path_.c_str(), O_RDWR | O_NOCTTY | O_TTY_INIT);
	int error = errno;

	if (itap_sock_ == -1) {
		std::cerr << "itap: app: do_setup(): itap TTY open(): error ";
		std::cerr << strerror(error) << std::endl;
		cleanup();
		return;
	}

	if (!isatty(itap_sock_)) {
		std::cerr << "itap: app: do_setup(): itap file is not a TTY" << std::endl;
		cleanup();
		return;
	}

	termios t;
	tcgetattr(itap_sock_, &t);
	// cfmakeraw(&t);

	/*	t.c_iflag &= ~(IGNBRK | IGNPAR | PARMRK | IGNCR | BRKINT | INPCK | ISTRIP | INLCR | ICRNL | IXON | IXOFF | IXANY | IMAXBEL);
	t.c_oflag &= ~(OPOST);
	t.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	t.c_cflag |= CS8;
	t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO | ECHOCTL);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 0; */

	t.c_lflag &= ~(ECHO | ECHOE | ICANON | IEXTEN | ISIG);
	t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
	t.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
	t.c_cflag |= CS8;
	t.c_oflag &= ~(OPOST);
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 10;

	cfsetspeed(&t, B38400);

	error = tcsetattr(itap_sock_, TCSANOW, &t);
	if (error) {
		error = errno;
		std::cerr << "itap: app: do_setup(): tcsetattr(): error ";
		std::cerr << strerror(error) << std::endl;
		cleanup();
		return;
	}

	fcntl(itap_sock_, F_SETFL, O_NONBLOCK);

	ev_itap_readable_.start(itap_sock_, ev::READ);

	ev_itap_ping_.set(0., 1.);
	ev_itap_timeout_.set(0., 5.);
}

void app::do_cleanup()
{
	if (itap_sock_ != -1) {
		close(itap_sock_);
		itap_sock_ = -1;
	}

	ev_itap_readable_.stop();
	ev_itap_timeout_.stop();
	ev_itap_ping_.stop();
}

void app::run()
{
	using namespace std::chrono_literals;
	for (int i = 0; i < 18; i++) {
		static const uint8_t itap_poll[2] = {0xFFU, 0xFFU};
		itap_reply(itap_poll, 2);
		std::this_thread::sleep_for(0.001s);
	}

	itap_ping(ev_itap_ping_, 0);

	ev_itap_timeout_.again();
	ev_itap_ping_.again();

	acked_ = true;

	loop_.run();
}

inline ssize_t app::itap_read(void* buf, size_t len)
{
	ssize_t count;
	count = read(itap_sock_, buf, len);

	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "itap: app: itap_read(): called but read() returned EAGAIN??" << std::endl;
			return count;
		}
		std::cerr << "itap: app: itap_read(): read(): error ";
		std::cerr << strerror(error) << std::endl;
		// TODO: cleanup
		cleanup();
		return count;
	}
	if (count == 0) {
		std::cerr << "itap: app: itap_read(): read() returned 0 byte??" << std::endl;
		cleanup();
		return -1;
	}
	return count;
}

inline void app::itap_reply(const void* buf, size_t len)
{
	ssize_t count = write(itap_sock_, buf, len);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "itap: app: itap_reply(): called but write() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "itap: app: itap_reply(): write(): error ";
		std::cerr << strerror(error) << std::endl;
		// TODO: cleanup
		cleanup();
		return;
	}
	if (count == 0) {
		std::cerr << "itap: app: itap_reply(): write() returned 0 byte??" << std::endl;
		return;
	}
	if ((size_t)count != len) {
		std::cerr << "itap: app: itap_reply(): write(): partial write??" << std::endl;
		return;
	}
	std::cout << "write packet " << std::to_string(len) << std::endl;

	// Supposedly this is needed.
	static const uint8_t msg_end = 0xFFU;
	count = write(itap_sock_, &msg_end, 1);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "itap: app: itap_reply(): called but write() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "itap: app: itap_reply(): write(): error ";
		std::cerr << strerror(error) << std::endl;
		// TODO: cleanup
		cleanup();
		return;
	}
	if (count == 0) {
		std::cerr << "itap: app: itap_reply(): write() returned 0 byte??" << std::endl;
		return;
	}
}

void app::itap_readable(ev::io&, int)
{
	std::cout << "itap readable" << std::endl;
	if (msg_length_ == 0) {
		auto count = itap_read(buf, 1);
		if (count == -1) return;
		msg_length_ = buf[0];
		msg_ptr_ = 1;
		std::cout << "itap message: " << std::to_string(msg_length_) << std::endl;
	}

	if (msg_length_ == 0xFFU || msg_length_ < 1) {// TODO: reset???
		msg_length_ = 0;
		ev_itap_timeout_.again();
		std::cout << "itap reset" << std::endl;
		return;
	}

	/*if (msg_length_ == 0x03U) {// pong
		msg_length_ = 0;
		ev_itap_timeout_.again();
		std::cout << "itap pong" << std::endl;
		return;
	}*/

	auto count = itap_read(&buf[msg_ptr_], msg_length_ + 1 - msg_ptr_);
	if (count == -1) return;

	msg_ptr_ += count;

	if (msg_ptr_ != msg_length_ + 1) {
		std::cout << "itap partial message read: " << std::to_string(msg_ptr_) << " " << std::to_string(msg_length_ + 1) << std::endl;
		return;// Wait for more
	}
	std::cout << "itap full message" << std::endl;

	auto* msg = (packet*)buf;

	switch (msg->type) {
	case I_HEADER: {
		if (tx_lock.test_and_set()) return;

		dgate::packet p;
		auto h = msg->header;
		if (std::memcmp(h.departure_rptr_cs, "DIRECT  ", 8) == 0) {
			// Terminal Mode sets DIRECT for all transmissions.
			// We have to re-write.
			std::string rptr = cs_;
			rptr[7] = module_;
			std::memcpy(h.departure_rptr_cs, rptr.c_str(), 8);
			rptr[7] = 'G';
			std::memcpy(h.destination_rptr_cs, rptr.c_str(), 8);
			h.set_crc(h.calc_crc());
		}
		p.type = dgate::P_HEADER;
		p.flags = dgate::P_LOCAL;
		p.module = module_;
		tx_id_ = rand_dist_(rand_gen_);
		p.header.id = tx_id_;
		p.header.h = h;

		dgate_reply(p, dgate::packet_header_size);

		packet tp;
		tp.length = 3;
		tp.type = O_HEADER_ACK;
		tp.ping = 0;
		itap_reply(&tp, 3);
		ev_itap_timeout_.again();
	} break;
	case I_VOICE: {
		if (!tx_lock.test()) return;

		dgate::packet p;
		p.flags = dgate::P_LOCAL;
		p.module = module_;
		if (msg->voice.seqno & 0x40U) {
			p.type = dgate::P_VOICE_END;
			p.voice_end.bit_errors = 0;
			p.voice_end.count = msg->voice.count;
			p.voice_end.id = tx_id_;
			p.voice_end.f = msg->voice.f;
			p.voice_end.seqno = msg->voice.seqno & 0x1F;
			dgate_reply(p, dgate::packet_voice_end_size);

			tx_lock.clear();// TODO: must clear after timeout, does AP
							// mode synthesize end packets?
		}
		else {
			p.type = dgate::P_VOICE;
			p.voice.count = msg->voice.count;
			p.voice.id = tx_id_;
			p.voice.seqno = msg->voice.seqno;
			p.voice.f = msg->voice.f;
			dgate_reply(p, dgate::packet_voice_size);
		}

		packet tp;
		tp.length = 3;
		tp.type = O_VOICE_ACK;
		tp.ping = 0;
		itap_reply(&tp, 3);
		ev_itap_timeout_.again();
	} break;
	case I_PONG:
		ev_itap_timeout_.again();
		break;
	case I_VOICE_ACK:
	case I_HEADER_ACK: {
		acked_ = true;
		ev_itap_timeout_.again();
		send_from_queue();
	} break;
	default: break;
	}
	msg_length_ = 0;
}

void app::itap_ping(ev::timer&, int)
{
	static const packet p = {2, O_PING, {}};

	if (msg_length_ != 0) return;

	itap_reply(&p, 2);
}

void app::itap_timeout(ev::timer&, int)
{
	cleanup();
}

void app::send_from_queue()
{
	if (queue_in_.empty()) return;

	auto p = queue_in_.front();
	queue_in_.pop();

	switch (p.type) {
	case dgate::P_HEADER:
		dgate_handle_header(p, dgate::packet_header_size);
		break;
	case dgate::P_VOICE:
		dgate_handle_voice(p, dgate::packet_voice_size);
		break;
	case dgate::P_VOICE_END:
		dgate_handle_voice(p, dgate::packet_voice_end_size);
		break;
	}
}

void app::dgate_handle_header(const dgate::packet& p, size_t)
{
	if (tx_lock.test() || p.module != module_) return;// ack

	if (!acked_) {
		queue_in_.push(p);
		return;
	}

	packet ip;
	ip.type = O_HEADER;
	ip.header = p.header.h;
	ip.length = 41U;// TODO: how long should this be... 41 doesnt
	// include CRC

	itap_reply(&ip, 41U);
	acked_ = false;
}

void app::dgate_handle_voice(const dgate::packet& p, size_t)
{
	if (tx_lock.test() || p.module != module_) return;// ack

	if (!acked_) {
		queue_in_.push(p);
		return;
	}

	packet ip;
	ip.type = O_VOICE;
	ip.voice.count = p.voice.count;
	ip.voice.seqno = p.voice.seqno;
	ip.voice.f = p.voice.f;
	ip.length = 16U;

	itap_reply(&ip, 16U);
	acked_ = false;
}

void app::dgate_handle_voice_end(const dgate::packet& p, size_t)
{
	if (tx_lock.test() || p.voice_end.id == tx_id_ || p.module != module_) return;// ack

	if (!acked_) {
		queue_in_.push(p);
		return;
	}

	packet ip;
	ip.type = O_VOICE;
	ip.voice.count = p.voice_end.count;
	ip.voice.f = p.voice_end.f;
	ip.voice.seqno = 0x40U;
	ip.length = 16U;

	itap_reply(&ip, 16U);
	acked_ = false;
}

}// namespace itap
