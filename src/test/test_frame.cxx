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

#include "../dv/types.h"
#include <array>
#include <cstring>
#include <iostream>
#include <string>

constexpr std::array<uint8_t, 12> sync_frame = {
    0xB2, 0x4D, 0x22, 0x48, 0xC0, 0x16, 0x28, 0x26, 0xC8, 0x55, 0x2D, 0x16, 
};

constexpr std::array<uint8_t, 12> voice_frame = {
    0xAE, 0xCC, 0x2A, 0x78, 0xE1, 0x13, 0x3C, 0x67, 0xC0, 0x30, 0x05, 0xBD, 
};

int main() {
std::array<uint8_t, 12> buffer = {
    0xAE, 0xCC, 0x2A, 0x78, 0xE1, 0x13, 0x3C, 0x67, 0xC0, 0x30, 0x05, 0xBD, 
};
	dv::rf_frame f;
	std::memcpy(&f, sync_frame.data(), sizeof(dv::rf_frame));

	// Test error correction
	f.ambe[0] ^= 0x81U;
	auto fd = f.decode();
	std::cout << std::to_string(fd.bit_errors) << std::endl;
	std::cout << std::to_string(fd.is_sync()) << std::endl;

	auto reconstruct = fd.encode();
	std::memcpy(buffer.data(), &reconstruct, sizeof(dv::rf_frame));
	for (int i = 0; i < sizeof(dv::rf_frame); i++) {
		std::cout << std::hex << (int)buffer[i] << " ";
	}
	std::cout << std::endl;
	for (int i = 0; i < sizeof(dv::rf_frame); i++) {
		std::cout << std::hex << (int)sync_frame[i] << " ";
	}
	std::cout << std::endl;

	std::cout << std::to_string(std::memcmp(sync_frame.data(), &reconstruct, sizeof(dv::rf_frame))) << std::endl;

	std::memcpy(&f, voice_frame.data(), sizeof(dv::rf_frame));
	fd = f.decode();
	std::cout << std::to_string(fd.bit_errors) << std::endl;
	std::cout << std::to_string(fd.is_sync()) << std::endl;
	std::cout << fd.data << std::endl;
}
