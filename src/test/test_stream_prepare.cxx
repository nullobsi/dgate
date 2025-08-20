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
#include "dv/stream.h"
#include "dv/types.h"
#include "dv/aprs.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <array>

using namespace std::chrono_literals;
using namespace std::string_literals;

int main() {
	dv::header h;
	h.flags[0] = 0;
	h.flags[1] = 0;
	h.flags[2] = 0;

	std::memcpy(h.destination_rptr_cs, "KO6JXH C", 8);
	std::memcpy(h.departure_rptr_cs, "KO6JXH G", 8);
	std::memcpy(h.companion_cs, "CQCQCQ  ", 8);
	std::memcpy(h.own_cs, "KPBS    ", 8);
	std::memcpy(h.own_cs_ext, "52P ", 4);

	h.set_crc(h.calc_crc());

	dv::stream s;
	s.header = h;
	s.tx_msg = "TEST TX"; // will get cut off
	s.serial_data = dv::encode_aprs_string("KPBS-0>API52,DSTAR*:!3241.78N/11703.84W[/TESTING APRS\r");
	//s.serial_data = "random unimportant serial data";
	// s.serial_data ="$$CRC75C8,KO6JXH-7>API52,DSTAR*:/200141z3239.44N/11657.83W[349/000/A=000694J.P. HT ID-52PLUS\r"s;

	s.prepare();

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	std::memcpy(addr.sun_path, "dgate.sock", 11);

	int fd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (fd == -1) return errno;

	int error = connect(fd, (sockaddr*)&addr, sizeof(sockaddr_un));
	if (error) return errno;


	dgate::packet p;
	dgate::packet pout;
	p.module = 'C';
	p.type = dgate::P_HEADER;
	p.header.id = 0xBEEF;
	p.header.h = h;
	p.flags = dgate::P_LOCAL;

	write(fd, &p, dgate::packet_header_size);
	int count = read(fd, &pout, dgate::packet_header_size);
	std::cout << (count == dgate::packet_header_size) << std::endl;

	p.type = dgate::P_VOICE;
	p.voice.count = 0;
	p.voice.seqno = 0;
	p.voice.id = 0xBEEF;
	std::vector<dv::rf_frame>::size_type i;
	for (i = 0; i < s.frames.size()-1; i++) {
		std::memcpy(&p.voice.f, &s.frames[i], sizeof(dv::rf_frame));
		write(fd, &p, dgate::packet_voice_size);
		count = read(fd, &pout, dgate::packet_voice_size);
		std::cout << count << " ";
		p.voice.count ++;
		p.voice.seqno = dgate::next_seqno(p.voice.seqno);
		std::this_thread::sleep_for(20ms);
	}
	std::cout << std::endl;

	int tmp = pout.voice.count;

	p.type = dgate::P_VOICE_END;
	p.voice_end.bit_errors = 0;
	p.voice_end.count = tmp+1;
	p.voice_end.id = 0xBEEF;

	std::memcpy(&p.voice_end.f, &s.frames[s.frames.size()-1], 12);
	write(fd, &p, dgate::packet_voice_end_size);
	count = read(fd, &pout, dgate::packet_voice_end_size);
	std::cout << std::to_string(pout.voice_end.count) << " " << std::to_string(pout.voice_end.bit_errors) << std::endl;
}
