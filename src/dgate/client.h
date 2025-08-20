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

#ifndef DGATE_CLIENT_H
#define DGATE_CLIENT_H

#include "dgate/dgate.h"
#include <ev++.h>
#include <string>
namespace dgate {

class client {
public:
	client(const std::string& dgate_socket_path, const std::string& cs);

	void setup();
	void cleanup();
	virtual void run();

	virtual ~client();

protected:
	virtual void do_setup();
	virtual void do_cleanup();

	virtual void dgate_handle_header(const packet& p, size_t len) = 0;
	virtual void dgate_handle_voice(const packet& p, size_t len) = 0;
	virtual void dgate_handle_voice_end(const packet& p, size_t len) = 0;

	void dgate_reply(const dgate::packet& p, size_t len);

	std::string cs_;

	ev::dynamic_loop loop_;

	int dgate_sock_;

private:
	std::string dgate_socket_path_;
	void dgate_readable(ev::io&, int);
	ev::io ev_dgate_readable_;
};

}// namespace dgate

#endif
