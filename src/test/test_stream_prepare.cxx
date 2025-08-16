#include "dgate/dgate.h"
#include "dv/stream.h"
#include "dv/types.h"
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

int main() {
	dv::header h;
	h.flags[0] = 0;
	h.flags[1] = 0;
	h.flags[2] = 0;

	std::memcpy(h.destination_rptr_cs, "DIRECT  ", 8);
	std::memcpy(h.departure_rptr_cs, "DIRECT  ", 8);
	std::memcpy(h.companion_cs, "       I", 8);
	std::memcpy(h.own_cs, "KO6JXH  ", 8);
	std::memcpy(h.own_cs_ext, "52P ", 4);

	h.crc_ccitt[0] = 0x04;
	h.crc_ccitt[1] = 0x74;

	dv::stream s;
	s.header = h;
	s.tx_msg = "This is a TX message."; // will get cut off
	s.serial_data ="$$CRC1FC0,KO6JXH-7>API52,DSTAR*:/141625z3239.47N/11657.79W[179/000/J.P. HT ID-52PLUS\r";
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

	int tmp = p.voice.count;

	p.type = dgate::P_VOICE_END;
	p.voice_end.bit_errors = 0;
	p.voice_end.count = tmp;
	p.voice_end.id = 0xBEEF;

	std::memcpy(&p.voice_end.f, &s.frames[s.frames.size()-1], 12);
	write(fd, &p, dgate::packet_voice_end_size);
	count = read(fd, &pout, dgate::packet_voice_end_size);
	std::cout << std::to_string(count) << " " << std::to_string(pout.voice_end.bit_errors) << std::endl;
}
