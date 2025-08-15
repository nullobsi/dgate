#ifndef DGATE_G2_H
#define DGATE_G2_H
#include "dv/types.h"
#include <cstdint>

namespace dgate {

// thx to qnetgateway Tom N7TAE
#pragma pack(push, 1)
struct g2_packet {
	char title[4];    //  0   "DSVT"
	uint8_t config;   //  4   0x10 is hdr 0x20 is vasd
	uint8_t flaga[3]; //  5   zeros
	uint8_t id;       //  8   0x20
	uint8_t flagb[3]; //  9   0x0 0x1 (A:0x3 B:0x1 C:0x2)
	uint16_t streamid;// 12
	uint8_t ctrl;     // 14   hdr: 0x80 vsad: framecounter (mod 21)
	union {
		dv::header header;// total 56
		dv::rf_frame frame;
		dv::rf_frame_end end_frame;
	};
};
#pragma pack(pop)

}// namespace dgate

#endif
