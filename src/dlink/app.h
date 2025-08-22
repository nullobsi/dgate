//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
//
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.
//

#ifndef DLINK_APP_H
#define DLINK_APP_H

#include "dgate/client.h"
#include "dv/types.h"
#include "xrf.h"
#include <ev++.h>
#include <sys/socket.h>
#include <unordered_map>
#include <unordered_set>
namespace dlink {

enum link_proto {
	L_LOCAL,
	L_DCS,
	L_XRF,
	L_REF,
};

enum link_status {
	L_UNLINKED,
	L_CONNECTING,
	L_LINKED,
};

class app;

struct module_state {
	bool tx_lock;
	link_proto link;
};

struct link {
	link(app* parent, ev::loop_ref loop, link_proto proto);

	app* parent;

	link_proto proto;
	link_status status;

	void timeout(ev::timer&, int);
	void heartbeat(ev::timer&, int);

	ev::timer ev_timeout_;
	ev::timer ev_heartbeat_;

	std::string reflector;
	char mod_from;
	char mod_to;

	sockaddr_storage addr;
};

class app : public dgate::client {
	friend link;

public:
	app(const std::string& dgate_socket_path, const std::string& cs, const std::string& reflectors_file, std::unordered_set<char> enabled_mods_);

protected:
	void do_setup() override;
	void do_cleanup() override;

	void dgate_handle_header(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice_end(const dgate::packet& p, size_t len) override;

private:
	void unlink(link_proto proto);
	void link_heartbeat(link_proto proto);

	void xrf_reply(const xrf_packet& p, size_t len);
	void xrf_link(char mod_from, const std::string& ref, char mod_to);
	void xrf_handle_packet(const xrf_packet& p, size_t len, const sockaddr_storage& from);
	void xrf_handle_header(const xrf_packet& p, size_t len, const sockaddr_storage& from);
	void xrf_handle_voice(const xrf_packet& p, size_t len, const sockaddr_storage& from);

	std::string cs_;

	void dcs_readable_v6(ev::io&, int);
	void xrf_readable_v6(ev::io&, int);
	void ref_readable_v6(ev::io&, int);
	ev::io ev_dcs_readable_v6_;
	ev::io ev_xrf_readable_v6_;
	ev::io ev_ref_readable_v6_;

	void dcs_readable_v4(ev::io&, int);
	void xrf_readable_v4(ev::io&, int);
	void ref_readable_v4(ev::io&, int);
	ev::io ev_dcs_readable_v4_;
	ev::io ev_xrf_readable_v4_;
	ev::io ev_ref_readable_v4_;

	int dcs_sock_v6_;
	int xrf_sock_v6_;
	int ref_sock_v6_;

	int dcs_sock_v4_;
	int xrf_sock_v4_;
	int ref_sock_v4_;

	link dcs_link_;
	link xrf_link_;
	link ref_link_;

	std::string reflectors_file_;
	std::unordered_map<std::string, std::string> reflectors_;
	std::unordered_map<char, module_state> modules_;
};

};// namespace dlink

#endif
