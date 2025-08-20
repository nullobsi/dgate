#ifndef DV_APRS_H
#define DV_APRS_H

#include <string>
namespace dv {

// The string must be given with a terminating CR (\r).
std::string encode_aprs_string(const std::string& aprs);

}

#endif
