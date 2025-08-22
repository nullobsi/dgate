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
#include "common/c++sock.h"
#include "dgate/dgate.h"
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace dlink {
void app::xrf_reply(const xrf_packet& p, size_t len)
{
	int result;
	if (xrf_link_.addr.ss_family == AF_INET) {
		result = sendto(xrf_sock_v4_, &p, len, 0, (sockaddr*)&xrf_link_.addr, sizeof(sockaddr_in));
	}
	else {
		result = sendto(xrf_sock_v6_, &p, len, 0, (sockaddr*)&xrf_link_.addr, sizeof(sockaddr_in6));
	}

	if (result == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dlink: xrf_reply(): sendto() returned EAGAIN!" << std::endl;
			// TODO: How often is this, do we need a resend queue?
		}
		else {
			std::cerr << "dlink: xrf_reply(): sendto(): error ";
			std::cerr << strerror(error) << std::endl;
			// TODO: wrap up
		}
	}
}

void app::xrf_link(char mod_from, const std::string& ref, char mod_to)
{
	std::cout << "xrf_link: " << ref << " has value " << reflectors_[ref] << std::endl;

	xrf_packet p;
	std::memcpy(p.link.from, cs_.c_str(), 8);

	p.link.mod_from = mod_from;
	p.link.mod_to = mod_to;
	p.link.null = 0;

	int error;
	struct addrinfo hints;
	struct addrinfo* servinfo = nullptr;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;

	error = getaddrinfo(reflectors_[ref].c_str(), "30001", &hints, &servinfo);
	if (error) {
		std::cerr << "gai error: " << gai_strerror(error) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		return;
	}

	xrf_link_.status = L_CONNECTING;
	std::memcpy(&xrf_link_.addr, servinfo->ai_addr, servinfo->ai_addrlen);
	xrf_link_.reflector = ref;
	xrf_link_.mod_from = mod_from;
	xrf_link_.mod_to = mod_to;

	xrf_link_.ev_timeout_.again();

	freeaddrinfo(servinfo);

	modules_[xrf_link_.mod_from].link = L_XRF;

	for (int i = 0; i < 5; i++) // Send multiple times (this is UDP after all)
		xrf_reply(p, sizeof(xrf_packet_link));
}

void app::xrf_readable_v4(ev::io&, int)
{
	sockaddr_storage from;
	socklen_t addrlen = sizeof(sockaddr_storage);

	xrf_packet p;
	auto count = recvfrom(xrf_sock_v4_, &p, sizeof(xrf_packet), 0, (sockaddr*)&from, &addrlen);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dlink: xrf_readable_v4: read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dlink: xrf_readable_v4: read(): ";
		std::cerr << strerror(error);
	}

	if (count == 0) {
		std::cerr << "dlink: xrf_readable_v4: zero packet, wtf?" << std::endl;
		return;
	}

	xrf_handle_packet(p, count, from);
}

void app::xrf_readable_v6(ev::io&, int)
{
	sockaddr_storage from;
	socklen_t addrlen = sizeof(sockaddr_storage);

	xrf_packet p;
	auto count = recvfrom(xrf_sock_v6_, &p, sizeof(xrf_packet), 0, (sockaddr*)&from, &addrlen);
	if (count == -1) {
		int error = errno;
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "dlink: xrf_readable_v6: read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "dlink: xrf_readable_v6: read(): ";
		std::cerr << strerror(error);
	}

	if (count == 0) {
		std::cerr << "dlink: xrf_readable_v6: zero packet, wtf?" << std::endl;
		return;
	}

	xrf_handle_packet(p, count, from);
}

void app::xrf_handle_packet(const xrf_packet& p, size_t len, const sockaddr_storage& from)
{
	std::cout << "xrf_handle_packet called" << std::endl;

	if (xrf_link_.status == L_UNLINKED) {
		std::cout << "xrf unlinked, not handling" << std::endl;
		return;
	}

	if (!sockaddr_addr_equal(&from, &xrf_link_.addr)) {
		std::cout << "sockaddr not the same, not handling" << std::endl;
		return;
	}

	if (p.is_ack() && xrf_link_.status == L_CONNECTING) {
		if (p.ack.ack[0] == 'N') {
			std::cout << "xrf link failed" << std::endl;
			// TODO: message
			unlink(L_XRF);
			return;
		}
		std::cout << "xrf link success" << std::endl;
		xrf_link_.ev_timeout_.again();
		xrf_link_.ev_heartbeat_.again();
		xrf_link_.status = L_LINKED;
	}
	else if (p.is_heartbeat() && xrf_link_.status == L_LINKED) {
		xrf_link_.ev_timeout_.again();
	}
	else if (p.is_header() && xrf_link_.status == L_LINKED) {
		xrf_link_.ev_timeout_.again();
		xrf_handle_header(p, len, from);
	}
	else if (p.is_voice() && xrf_link_.status == L_LINKED) {
		xrf_link_.ev_timeout_.again();
		xrf_handle_voice(p, len, from);
	}
}

void app::xrf_handle_header(const xrf_packet& p, size_t, const sockaddr_storage&)
{
	dgate::packet dp;

	dp.module = xrf_link_.mod_from;
	dp.type = dgate::P_HEADER;
	dp.header.h = p.header.header;
	dp.header.id = p.header.streamid;

	// RPT1: RPTR   A
	// RTP2: RPTR   G
	std::string cs = cs_;
	cs[7] = dp.module;
	std::memcpy(dp.header.h.departure_rptr_cs, cs.c_str(), 8);
	cs[7] = 'G';
	std::memcpy(dp.header.h.destination_rptr_cs, cs.c_str(), 8);

	dp.header.h.set_crc(dp.header.h.calc_crc());

	// TODO: does URCALL need to be overwritten too?

	send(dgate_sock_, &dp, dgate::packet_header_size, 0);
}

void app::xrf_handle_voice(const xrf_packet& p, size_t, const sockaddr_storage&)
{
	dgate::packet dp;

	dp.module = xrf_link_.mod_from;
	if (p.voice.seqno & 0x40U) {// Ending
		dp.type = dgate::P_VOICE_END;
		dp.voice_end.bit_errors = 0;
		dp.voice_end.count = 0;
		dp.voice_end.f = p.voice.frame;
		dp.voice_end.id = p.voice.streamid;
		dp.voice_end.seqno = p.voice.seqno & 0x1F;
	}
	else {
		dp.type = dgate::P_VOICE;
		dp.voice.count = 0;
		dp.voice.seqno = p.voice.seqno;
		dp.voice.f = p.voice.frame;
		dp.voice.id = p.voice.streamid;
	}

	send(dgate_sock_, &dp, p.voice.seqno & 0x40U ? dgate::packet_voice_end_size : dgate::packet_voice_size, 0);
}
}// namespace dlink
