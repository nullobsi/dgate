#ifndef DGATE_CXX_SOCK_H
#define DGATE_CXX_SOCK_H

#include <string>
#include <iostream>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

static inline constexpr bool sockaddr_addr_equal(const sockaddr_storage* one, const sockaddr_storage* two)
{
	if (one->ss_family != two->ss_family) return false;

	std::string sa("A");
	std::string sb("B");

	switch (one->ss_family) {
	case AF_INET: {
		auto a = reinterpret_cast<const sockaddr_in*>(one);
		auto b = reinterpret_cast<const sockaddr_in*>(two);
		sa.resize(INET_ADDRSTRLEN);
		sb.resize(INET_ADDRSTRLEN);
		inet_ntop(AF_INET, &a->sin_addr, sa.data(), INET_ADDRSTRLEN);
		inet_ntop(AF_INET, &b->sin_addr, sb.data(), INET_ADDRSTRLEN);
		std::cout << sa << " " << sb << std::endl;
		return sa == sb;
	} break;

	case AF_INET6: {
		auto a = reinterpret_cast<const sockaddr_in6*>(one);
		auto b = reinterpret_cast<const sockaddr_in6*>(two);
		sa.resize(INET6_ADDRSTRLEN);
		sb.resize(INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, &a->sin6_addr, sa.data(), INET6_ADDRSTRLEN);
		inet_ntop(AF_INET6, &b->sin6_addr, sb.data(), INET6_ADDRSTRLEN);
		return sa == sb;
	} break;
	}
	return false;
}

#endif
