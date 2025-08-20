#ifndef DGATE_XRF_H
#define DGATE_XRF_H

#include "dv/types.h"
#include <cstring>
namespace dlink {

struct xrf_packet_heartbeat {
	char from[9];// Null-terminated, shouldn't include module
};

struct xrf_packet_link {
	char from[8];// Doesn't need module
	char mod_from;
	char mod_to;// Set to ' ' to unlink
	char null;
};

struct xrf_packet_link_ack {
	char from[8];// Doesn't need module
	char mod_from;
	char mod_to;
	char ack[3];// Could also be NAK if failed to connect
	char null;
};

// It seems that XRF uses the same g2 packet structure.
// just a bit modified.
struct xrf_packet_header {
	char title[4];    //  0   "DSVT"
	uint8_t config;   //  4   0x10
	uint8_t flaga[3]; //  5   zeros
	uint8_t id;       //  8   0x20
	uint8_t flagb[3]; //  9   0x0 0x1 MOD
	uint16_t streamid;// 12
	uint8_t ctrl;     // 14   0x80
	dv::header header;// total 56
};

struct xrf_packet_voice {
	char title[4];    //  0   "DSVT"
	uint8_t config;   //  4   0x20
	uint8_t flaga[3]; //  5   zeros
	uint8_t id;       //  8   0x20
	uint8_t flagb[3]; //  9   0x0 0x1 (A:0x3 B:0x1 C:0x2)
	uint16_t streamid;// 12
	uint8_t seqno;    // 14   framecounter (mod 21)
	dv::rf_frame frame;
};

struct xrf_packet {
	union {
		xrf_packet_header header;
		xrf_packet_voice voice;
		xrf_packet_heartbeat heartbeat;
		xrf_packet_link link;
		xrf_packet_link_ack ack;
	};

	inline bool is_heartbeat() const {
		return heartbeat.from[8] == 0;
	}

	inline bool is_ack() const {
		return ack.null == 0 && ack.ack[2] == 'K';
	}

	inline bool is_header() const {
		return std::memcmp(header.title, "DSVT", 4) == 0 && header.config == 0x10 && header.id == 0x20U && header.ctrl == 0x80;
	}

	inline bool is_voice() const {
		return std::memcmp(voice.title, "DSVT", 4) == 0 && voice.config == 0x20 && voice.id == 0x20;
	}
};

}// namespace dlink

#endif
