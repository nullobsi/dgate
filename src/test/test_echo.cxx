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

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	std::memcpy(addr.sun_path, "dgate.sock", 11);

	int fd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (fd == -1) return errno;

	int error = connect(fd, (sockaddr*)&addr, sizeof(sockaddr_un));
	if (error) return errno;

	while (p.type != dgate::P_VOICE_END) {
		read(fd, &p, sizeof(dgate::packet));
		packets.push_back(p);
	}

	std::this_thread::sleep_for(500ms);

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
