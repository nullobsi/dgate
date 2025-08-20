//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
// 
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
//   3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "dgate/dgate.h"
#include "dv/aprs.h"
#include "dv/stream.h"
#include "dv/types.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <queue>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using namespace std::string_literals;

int main()
{
	std::vector<dgate::packet> packets;

	dgate::packet p;
	p.type = dgate::P_HEADER;

	// Open d-gate socket
	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	std::memcpy(addr.sun_path, "dgate.sock", 11);

	int fd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (fd == -1) return errno;

	int error = connect(fd, (sockaddr*)&addr, sizeof(sockaddr_un));
	if (error) return errno;


	// Capture a transmission
	while (p.type != dgate::P_VOICE_END) {
		read(fd, &p, sizeof(dgate::packet));
		packets.push_back(p);
	}

	// Wait a while
	std::this_thread::sleep_for(500ms);

	// Modify stream data
	dv::stream s;

	uint16_t sid = packets[0].header.id;

	s.header = packets[0].header.h;
	for (auto i = 1; i < packets.size() - 1; i++) {
		s.frames.push_back(packets[i].voice.f);
	}
	s.frames.push_back(packets[packets.size() - 1].voice_end.f);

	s.tx_msg = "NEW TX MSG";
	s.serial_data = dv::encode_aprs_string("KO6JXH-7>API52,DSTAR*:!3241.78N/11703.84W[/TESTING APRS\r");

	s.prepare();

	p.type = dgate::P_HEADER;
	p.header.h = s.header;
	p.header.id = sid;
	std::memcpy(p.header.h.departure_rptr_cs, "KO6JXH C", 8);
	std::memcpy(p.header.h.destination_rptr_cs, "KO6JXH G", 8);
	p.header.h.set_crc(p.header.h.calc_crc());

	write(fd, &p, dgate::packet_header_size);
	read(fd, &p, dgate::packet_header_size);

	std::this_thread::sleep_for(19ms);

	dgate::packet pout;

	p.type = dgate::P_VOICE;
	p.voice.count = 0;
	p.voice.seqno = 0;
	p.voice.id = sid;

	for (auto i = 0; i < s.frames.size() - 1; i++) {
		p.voice.f = s.frames[i];
		write(fd, &p, dgate::packet_voice_size);
		read(fd, &pout, dgate::packet_voice_size);
		p.voice.count++;
		p.voice.seqno = (p.voice.seqno + 1) % 21;
		std::this_thread::sleep_for(19ms);
	}

	p.type = dgate::P_VOICE_END;
	p.voice_end.id = sid;
	p.voice_end.f = s.frames[s.frames.size() - 1];
	write(fd, &p, dgate::packet_voice_end_size);
	read(fd, &pout, dgate::packet_voice_end_size);

	std::cout << std::to_string(p.voice_end.count) << " " << std::to_string(p.voice_end.bit_errors) << std::endl;
}
