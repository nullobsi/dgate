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
