#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

constexpr std::array<uint8_t, 41> data = {
    0x00, 0x00, 0x00, 0x44, 0x49, 0x52, 0x45, 0x43, 0x54, 0x20, 0x20, 0x44, 0x49, 0x52, 0x45, 0x43, 
    0x54, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x49, 0x4B, 0x4F, 0x36, 0x4A, 0x58, 
    0x48, 0x20, 0x20, 0x35, 0x32, 0x50, 0x20, 0x04, 0x74, 
};

int main() {
	uint16_t crc_tab_le[256] = {0};

	uint16_t crc = 1;
	int i = 128;

	do {
		if (crc & 1) {
			crc = (crc >> 1) ^ 0x8408;
		} else {
			crc = (crc >> 1);
		}
		for (int j = 0; j < 256; j += 2*i) {
			crc_tab_le[i+j] = crc ^ crc_tab_le[j];
		}
		i = i >> 1;
	} while (i > 0);

	// The D-Star CRC for the header is little-endian CRC-CCITT with a
	// starting value of 0xFFFF and ends by 
	crc = 0xFFFFU;
	for (int i = 0; i < 39; i++) {
		crc = (crc >> 8) ^ crc_tab_le[data[i] ^ (uint8_t)(crc & 0x00FF)];
	}
	crc = ~crc;
	std::cout << crc << std::endl;

	std::cout << std::setfill('0');
	for (int i = 0; i < 256; i++) {
		std::cout << std::internal << std::setw(6) << std::hex << std::showbase << std::uppercase << crc_tab_le[i] << "U, ";
	}
	std::cout << std::endl;
	return 1;

}
