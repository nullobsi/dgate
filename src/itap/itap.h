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
