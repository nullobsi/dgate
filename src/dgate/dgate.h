#ifndef DGATE_DGATE_H
#define DGATE_DGATE_H
#include "dv/types.h"
#include <cstdint>

namespace dgate {

enum packet_type : uint8_t {
	P_VOICE = 0x20U,
	P_VOICE_END = 0x21U,
	P_HEADER = 0x10U,
};

enum packet_flags : uint8_t {
	P_LOCAL = 0x01U,
};

static constexpr char packet_title[] = "DGTE";

static inline constexpr uint8_t next_seqno(uint8_t in)
{
	return (in + 1) % 21;
}

static inline constexpr uint8_t prev_seqno(int8_t in)
{
	return (in - 1) % 21;
}

#pragma pack(push, 1)
struct packet_voice {
	uint16_t id;
	uint8_t count;
	uint8_t seqno;
	dv::rf_frame f;
};

struct packet_voice_end {
	uint16_t id;
	uint8_t count;
	uint8_t seqno;
	dv::rf_frame f;
	uint32_t bit_errors;
};

struct packet_header {
	uint16_t id;
	dv::header h;
};

struct packet {
	char title[4];// DGTE
	char module;
	packet_type type;
	packet_flags flags;
	uint8_t reserved_;// unused
	union {
		packet_voice voice;
		packet_voice_end voice_end;
		packet_header header;
	};

	packet();
};
#pragma pack(pop)

static constexpr std::size_t packet_voice_size = 8 + sizeof(packet_voice);
static constexpr std::size_t packet_voice_end_size = 8 + sizeof(packet_voice_end);
static constexpr std::size_t packet_header_size = 8 + sizeof(packet_header);

}// namespace dgate

#endif
