#include "aprs.h"
#include "crc.h"

#include <cstdint>
#include <format>

namespace dv {
std::string encode_aprs_string(const std::string& aprs)
{
	uint16_t crc = calc_crc((const uint8_t*)aprs.data(), aprs.size());

	return std::format("$$CRC{:-04X},", crc) + aprs;
}

}// namespace dv
