#include "dgate.h"
#include <cstring>

namespace dgate {

packet::packet() : module(), type() {
	std::memcpy(title, packet_title, 4);
}

}
