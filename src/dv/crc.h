#ifndef DV_CRC_H
#define DV_CRC_H
#include <cstddef>
#include <cstdint>

namespace dv {

uint16_t calc_crc(const uint8_t* data, size_t len);

}

#endif
