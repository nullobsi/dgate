#include "app.h"
#include "common/c++sock.h"
#include "dgate/dgate.h"
#include "dlink/xrf.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <regex>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

namespace dlink {
static inline constexpr void try_close(int& fd)
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

static inline int try_create_socket(const char* port, int family, int* fd)
{
	int error;
	struct addrinfo hints;
	struct addrinfo* servinfo = nullptr;

	// Listen to v6 socket
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(nullptr, port, &hints, &servinfo);
	if (error) {
		std::cerr << "gai error: " << gai_strerror(error) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		return -1;
	}

	*fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	error = errno;

	if (*fd == -1) {
		std::cerr << "dlink: socket(): could not create socket: ";
		std::cerr << strerror(error) << std::endl;
		freeaddrinfo(servinfo);
		return -1;
	}

	if (family == AF_INET6) {
		// do NOT hybrid bind
		int sockopt = 1;
		setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY, &sockopt, sizeof(sockopt));
	}

	error = bind(*fd, servinfo->ai_addr, servinfo->ai_addrlen);
	if (error) {
		error = errno;
		std::cerr << "dlink: bind(): " << strerror(error) << std::endl;
		freeaddrinfo(servinfo);
		return -1;
	}
	freeaddrinfo(servinfo);

	return 0;
}

link::link(app* parent_, ev::loop_ref loop_, link_proto p) : parent(parent_), proto(p), status(L_UNLINKED), ev_timeout_(loop_), ev_heartbeat_(loop_)
{
	ev_timeout_.set<link, &link::timeout>(this);
	ev_heartbeat_.set<link, &link::heartbeat>(this);
}

void link::heartbeat(ev::timer&, int)
{
	parent->link_heartbeat(proto);
}

void link::timeout(ev::timer&, int)
{
	std::cout << "link timeout" << std::endl;
	parent->unlink(proto);
}

void app::do_cleanup()
{
	try_close(dcs_sock_v6_);
	try_close(xrf_sock_v6_);
	try_close(ref_sock_v6_);

	try_close(dcs_sock_v4_);
	try_close(xrf_sock_v4_);
	try_close(ref_sock_v4_);
}

app::app(const std::string& dgate_socket_path, const std::string& cs, const std::string& reflectors_file, std::unordered_set<char> enabled_mods_)
	: dgate::client(dgate_socket_path, cs),
	  ev_dcs_readable_v6_(loop_), ev_xrf_readable_v6_(loop_), ev_ref_readable_v6_(loop_),
	  ev_dcs_readable_v4_(loop_), ev_xrf_readable_v4_(loop_), ev_ref_readable_v4_(loop_),
	  dcs_sock_v6_(-1), xrf_sock_v6_(-1), ref_sock_v6_(-1),
	  dcs_sock_v4_(-1), xrf_sock_v4_(-1), ref_sock_v4_(-1),
	  dcs_link_(this, loop_, L_DCS), xrf_link_(this, loop_, L_XRF), ref_link_(this, loop_, L_REF), reflectors_file_(reflectors_file)
{
	cs_.resize(8, ' ');

	//ev_dcs_readable_v6_.set<app, &app::dcs_readable_v6>(this);
	ev_xrf_readable_v6_.set<app, &app::xrf_readable_v6>(this);
	//ev_ref_readable_v6_.set<app, &app::ref_readable_v6>(this);

	//ev_dcs_readable_v4_.set<app, &app::dcs_readable_v4>(this);
	ev_xrf_readable_v4_.set<app, &app::xrf_readable_v4>(this);
	//ev_ref_readable_v4_.set<app, &app::ref_readable_v4>(this);

	for (char c : enabled_mods_) {
		modules_[c] = {false, L_LOCAL};
	}
}

void app::do_setup()
{
	int error;

	std::ifstream hosts;
	hosts.open(reflectors_file_);

	for (std::string line; std::getline(hosts, line);) {
		std::istringstream iss(line);
		std::string host;
		std::string ip;
		iss >> host;
		iss >> ip;

		reflectors_[host] = ip;
	}

	std::cout << std::to_string(reflectors_.size()) << " reflectors loaded." << std::endl;

	error = try_create_socket("30001", AF_INET6, &xrf_sock_v6_);
	if (error) {
		cleanup();
		return;
	}

	error = try_create_socket("30001", AF_INET, &xrf_sock_v4_);
	if (error) {
		cleanup();
		return;
	}

	fcntl(xrf_sock_v6_, F_SETFL, O_NONBLOCK);

	fcntl(xrf_sock_v4_, F_SETFL, O_NONBLOCK);

	ev_xrf_readable_v6_.start(xrf_sock_v6_, ev::READ);

	ev_xrf_readable_v4_.start(xrf_sock_v4_, ev::READ);

	xrf_link_.ev_timeout_.set(0., 5.);  // TODO: how often are heartbeats
	xrf_link_.ev_heartbeat_.set(0., 1.);// TODO: how often are heartbeats
}

void app::unlink(link_proto proto)
{
	std::cout << "unlink: ";
	switch (proto) {
	case L_DCS: {
		// TODO
	} break;

	case L_XRF: {
		xrf_link_.status = L_UNLINKED;
		xrf_link_.ev_timeout_.stop();
		xrf_link_.ev_heartbeat_.stop();

		modules_[xrf_link_.mod_from].link = L_LOCAL;

		xrf_packet p;
		p.link.mod_from = xrf_link_.mod_from;
		p.link.mod_to = ' ';
		p.link.null = 0;
		std::memcpy(p.link.from, cs_.c_str(), 8);
		xrf_reply(p, sizeof(xrf_packet_link));
		std::cout << " xrf.";
	} break;

	case L_REF: {
		// TODO
	} break;
	case L_LOCAL:
		break;
	}
	std::cout << std::endl;
}

void app::link_heartbeat(link_proto proto)
{
	std::cout << "heartbeat: ";
	switch (proto) {
	case L_DCS: {
		// TODO
	} break;

	case L_XRF: {
		xrf_packet p;
		std::memcpy(p.heartbeat.from, cs_.c_str(), 9);
		xrf_reply(p, sizeof(xrf_packet_heartbeat));
		std::cout << " xrf.";
	} break;

	case L_REF: {
		// TODO
	} break;
	case L_LOCAL:
		break;
	}
	std::cout << std::endl;
}

static const std::regex REFLECTOR_LINK_UR_CS = std::regex("^(DCS|XRF|REF|XLX)([0-9]{3})([A-Z])L$", std::regex_constants::ECMAScript | std::regex_constants::optimize);

void app::dgate_handle_header(const dgate::packet& p, size_t)
{
	// Ignore non-local packets
	if (!(p.flags & dgate::P_LOCAL)) return;
	if (!modules_.contains(p.module)) return;

	auto& h = p.header.h;
	std::string ur_cs = std::string(h.companion_cs, 8);
	std::string rpt2 = std::string(h.destination_rptr_cs, 8);

	std::smatch match;

	if (modules_[p.module].link == L_LOCAL && std::regex_match(ur_cs, match, REFLECTOR_LINK_UR_CS)) {
		if (match[1] == "DCS") {
			// TODO
		}
		else if (match[1] == "XRF" || match[1] == "XLX") {
			std::cout << "XRF link request: " << match[0] << std::endl;
			xrf_link(p.module, ur_cs.substr(0, 6), ur_cs[6]);
		}
		else if (match[1] == "REF") {
			// TODO
		}
	}
	else if (modules_[p.module].link != L_LOCAL && ur_cs == "       U") {
		std::cout << "unlink request: " << ur_cs << std::endl;
		unlink(modules_[p.module].link);
	}
	else if (modules_[p.module].link != L_LOCAL && ur_cs == "CQCQCQ  " && rpt2.ends_with('G')) {// Probably just a normal header to send off
		switch (modules_[p.module].link) {
		case L_DCS: {
			// TODO
		} break;

		case L_XRF: {
			std::cout << "XRF start TX" << std::endl;
			xrf_packet xp;
			std::memcpy(xp.header.title, "DSVT", 4);

			xp.header.config = 0x10U;
			xp.header.flaga[0] = 0;
			xp.header.flaga[1] = 0;
			xp.header.flaga[2] = 0;
			xp.header.id = 0x20;
			xp.header.flagb[0] = 0;
			xp.header.flagb[0] = 1;
			xp.header.flagb[0] = 1;// TODO: does this even matter
			xp.header.streamid = p.header.id;
			xp.header.ctrl = 0x80U;
			xp.header.header = p.header.h;
			xrf_reply(xp, sizeof(xrf_packet_header));
		} break;

		case L_REF: {
			// TODO
		} break;
		case L_LOCAL:
			break;
		}
	}
}

void app::dgate_handle_voice(const dgate::packet& p, size_t)
{
	// Ignore non-local packets
	if (!(p.flags & dgate::P_LOCAL)) return;
	if (!modules_.contains(p.module)) return;
	if (modules_[p.module].link == L_LOCAL) return;

	switch (modules_[p.module].link) {
	case L_DCS: {
		// TODO
	} break;

	case L_XRF: {
		xrf_packet xp;
		std::memcpy(xp.voice.title, "DSVT", 4);
		xp.voice.config = 0x20U;
		xp.voice.flaga[0] = 0;
		xp.voice.flaga[1] = 0;
		xp.voice.flaga[2] = 0;
		xp.voice.id = 0x20;
		xp.voice.flagb[0] = 0;
		xp.voice.flagb[0] = 1;
		xp.voice.flagb[0] = 1;// TODO: does this even matter
		xp.voice.streamid = p.voice.id;
		xp.voice.seqno = p.voice.seqno;
		xp.voice.frame = p.voice.f;
		xrf_reply(xp, sizeof(xrf_packet_voice));
		std::cout << "XRF voice send" << std::endl;
	} break;

	case L_REF: {
		// TODO
	} break;
	case L_LOCAL:
		break;
	}
}

void app::dgate_handle_voice_end(const dgate::packet& p, size_t)
{
	// Ignore non-local packets
	if (!(p.flags & dgate::P_LOCAL)) return;
	if (!modules_.contains(p.module)) return;
	if (modules_[p.module].link == L_LOCAL) return;

	switch (modules_[p.module].link) {
	case L_DCS: {
		// TODO
	} break;

	case L_XRF: {
		xrf_packet xp;
		std::memcpy(xp.voice.title, "DSVT", 4);
		xp.voice.config = 0x20U;
		xp.voice.flaga[0] = 0;
		xp.voice.flaga[1] = 0;
		xp.voice.flaga[2] = 0;
		xp.voice.id = 0x20;
		xp.voice.flagb[0] = 0;
		xp.voice.flagb[0] = 1;
		xp.voice.flagb[0] = 1;// TODO: does this even matter
		xp.voice.streamid = p.voice_end.id;
		xp.voice.seqno = 0x40U | p.voice_end.seqno;// TODO: should probably synthesize this
		xp.voice.frame = p.voice_end.f;
		xrf_reply(xp, sizeof(xrf_packet_voice));
		std::cout << "XRF voice end" << std::endl;
	} break;

	case L_REF: {
		// TODO
	} break;
	case L_LOCAL:
		break;
	}
}

}// namespace dlink
