#include "../dv/types.h"
#include <cstring>
#include <iostream>

int main() {
	dv::header h;
	h.flags[0] = 0;
	h.flags[1] = 0;
	h.flags[2] = 0;

	std::memcpy(h.destination_rptr_cs, "DIRECT  ", 8);
	std::memcpy(h.departure_rptr_cs, "DIRECT  ", 8);
	std::memcpy(h.companion_cs, "       I", 8);
	std::memcpy(h.own_cs, "KO6JXH  ", 8);
	std::memcpy(h.own_cs_ext, "52P ", 4);

	h.crc_ccitt[0] = 0x04;
	h.crc_ccitt[1] = 0x74;

	std::cout << h.get_crc() << std::endl;
	std::cout << h.calc_crc() << std::endl;

	std::cout << h.verify() << std::endl;
}
