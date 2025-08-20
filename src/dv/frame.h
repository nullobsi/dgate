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

#ifndef DV_FRAME_H
#define DV_FRAME_H
#include <cstdint>

namespace dv {

enum miniheader_flags {
	F_DATA = 0x30,
	F_TXMSG = 0x40,
	F_HEADER = 0x50,
	F_FASTDATA_1 = 0x80,
	F_FASTDATA_2 = 0x90,
	F_DSQL = 0xC0,
	F_EMPTY = 0x66,
};

struct frame;

// Encoded D-STAR voice + data frame. This is exactly what would get
// sent over RF.
#pragma pack(push, 1)
struct rf_frame {
	uint8_t ambe[9];// Proprietary AMBE data with FEC.
	uint8_t data[3];// Scrambled data.

	bool is_sync() const;
	bool is_preend() const;
	bool is_end() const;
	frame decode() const;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct rf_frame_end {
	rf_frame f;
	uint8_t end_seq[3];// Extra data sent at the end of a transmission.
};
#pragma pack(pop)

// Since we can't really decode AMBE data to voice, this is just rf_frame but
// with the data field unscrambled and the AMBE data decoded (error
// correction applied).
struct frame {
	// These are technically 12-bit fields.
	uint16_t ambe_decode_1;
	uint16_t ambe_decode_2;
	uint8_t ambe[3];// The remaining untouched AMBE data.
	char data[3];   // Unscrambled data.

	uint8_t bit_errors;// Number of bit errors from the process of decoding AMBE data.

	bool is_sync() const;
	bool is_preend() const;
	bool is_end() const;
	rf_frame encode() const;
};

static constexpr uint8_t rf_ambe_null[9] = {0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U};
static constexpr uint8_t rf_data_scram[3] = {0x70U, 0x4FU, 0x93U};
static constexpr uint8_t rf_data_null[3] = {F_EMPTY ^ 0x70U, F_EMPTY ^ 0x4FU, F_EMPTY ^ 0x93U};
static constexpr uint8_t rf_data_sync[3] = {0x55U, 0x2DU, 0x16U};
static constexpr uint8_t rf_data_preend[3] = {0x55U, 0x55U, 0x55U};
static constexpr uint8_t rf_ambe_end[9] = {0x55, 0xC8, 0x7A, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};

static inline constexpr void scram_data(uint8_t out[3], const uint8_t in[3])
{
	out[0] = rf_data_scram[0] ^ in[0];
	out[1] = rf_data_scram[1] ^ in[1];
	out[2] = rf_data_scram[2] ^ in[2];
}

}// namespace dv

#endif
