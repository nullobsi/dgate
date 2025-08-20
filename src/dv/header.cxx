#include "header.h"
#include "crc.h"

namespace dv {

uint16_t header::calc_crc() const
{
	const auto data = static_cast<const uint8_t*>(&this->flags[0]);

	return dv::calc_crc(data, 39);
}

void header::set_crc(uint16_t crc)
{
	// Little Endian packing
	uint8_t lsb = crc & 0xFFU;
	uint8_t msb = (crc >> 8) & 0xFFU;

	crc_ccitt[0] = lsb;
	crc_ccitt[1] = msb;
}

uint16_t header::get_crc() const
{
	uint16_t lsb = crc_ccitt[0];
	uint16_t msb = crc_ccitt[1];

	msb <<= 8;
	msb &= 0xFF00;

	return msb | lsb;
};

bool header::verify() const
{
	return get_crc() == calc_crc();
}

}// namespace dv
