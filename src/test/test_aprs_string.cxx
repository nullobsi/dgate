#include <cstdint>
#include <string>
#include <iostream>
#include "dv/aprs.h"

int main()
{
	// should be: $$CRC2DBE,KO6JXH-7>API52,DSTAR*:/200241z3239.44N/11657.83W[/J.P. HT ID-52PLUS

	std::string aprs = "KO6JXH-7>API52,DSTAR*:/200241z3239.44N/11657.83W[/J.P. HT ID-52PLUS\r";

	std::cout << dv::encode_aprs_string(aprs) << std::endl;
}
