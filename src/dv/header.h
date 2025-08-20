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

#ifndef DV_HEADER_H
#define DV_HEADER_H
#include <cstdint>

namespace dv {

enum header_flags_a {
	H_DATA = 0x10000000,
	H_REPEATER = 0x01000000,
	H_INTERRUPT = 0x00100000,
	H_CONTROL = 0x00010000,
	H_URGENT = 0x00001000,

	// These flags are compound. Discard the top bits and compare the
	// least significant three.
	H_RPTR_CTRL = 0x111,
	H_AUTO_REPLY = 0x110,
	H_RESEND = 0x100,
	H_ACK = 0x011,
	H_NO_REPLY = 0x010,
	H_RELAY_UNAVAIL = 0x001,
};

struct header;

// This is the 660 bit header as sent over RF.
#pragma pack(push, 1)
struct rf_header {
	// This should technically be 82.5 bytes.
	uint8_t data[83];

	header decode() const;
};
#pragma pack(pop)

// Decoded D-STAR header.
#pragma pack(push, 1)
struct header {
	// Only flag 0 (A) is used. The other two are reserved in the d-star
	// specification.
	uint8_t flags[3];

	char destination_rptr_cs[8];
	char departure_rptr_cs[8];
	char companion_cs[8];
	char own_cs[8];
	char own_cs_ext[4];

	// This value is transferred as a little-endian value.
	uint8_t crc_ccitt[2];

	uint16_t calc_crc() const;
	void set_crc(uint16_t crc);
	uint16_t get_crc() const;

	bool verify() const;

	rf_header encode() const;
};
#pragma pack(pop)

}// namespace dv

#endif
