#ifndef DGATE_DCS_H
#define DGATE_DCS_H

#include <cstdint>
namespace dgate {

#pragma pack(push, 1)
struct dcs_packet_heartbeat {
	char from[8];
	char unknown;
	char to[8];
};

struct dcs_packet_unlink {
	char from[8];
	char from_mod;
	char space;
	char null;
	char to[8];
};

struct dcs_packet_link {
};
#pragma pack(pop)

}// namespace dgate

#endif
