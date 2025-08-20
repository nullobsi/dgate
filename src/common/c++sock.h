//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
//
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
//   3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef DGATE_CXX_SOCK_H
#define DGATE_CXX_SOCK_H

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>

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
