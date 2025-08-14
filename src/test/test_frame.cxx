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
