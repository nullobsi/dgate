#include "tables.h"
#include <cstddef>
#include <cstdint>

namespace dv {

uint16_t calc_crc(const uint8_t* data, size_t len)
{
	uint16_t crc = 0xFFFFU;

	// The D-Star CRC is little-endian CRC-CCITT with a
	// starting value of 0xFFFF and ends by inverting the bits in the
	// calculated remainder.
	crc = 0xFFFFU;
	for (size_t i = 0; i < len; i++) {
		crc = (crc >> 8) ^ crc_tab_ccitt_le[data[i] ^ (uint8_t)(crc & 0x00FF)];
	}
	crc = ~crc;

	return crc;
}

};// namespace dv
