#ifndef ITAP_ITAP_H
#define ITAP_ITAP_H

#include "dv/types.h"
#include <cstdint>
namespace itap {

enum packet_type : uint8_t {
	I_PONG = 0x03U,
	I_HEADER = 0x10U,
	I_HEADER_ACK = 0x21U,
	I_VOICE = 0x12U,
	I_VOICE_ACK = 0x23U,

	O_PING = 0x02U,
	O_HEADER_ACK = 0x11U,
	O_VOICE_ACK = 0x13U,
	O_HEADER = 0x20U,
	O_VOICE = 0x22U,
};

#pragma pack(push, 1)
struct packet_voice {
	uint8_t count;
	uint8_t seqno;
	dv::rf_frame f;
};

struct packet {
	uint8_t length;
	packet_type type;
	union {
		uint8_t ping;
		packet_voice voice;
		dv::header header;
	};
};
#pragma pack(pop)

}// namespace itap

#endif
