#include "client.h"
#include "dgate/dgate.h"
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

namespace dgate {

client::client(const std::string& dgate_socket_path, const std::string& cs)
	: cs_(cs), loop_(), dgate_sock_(-1), ev_dgate_readable_(loop_)
{
	cs_.resize(8);
	ev_dgate_readable_.set<client, &client::dgate_readable>(this);
}

void client::cleanup()
{
	if (dgate_sock_ != -1) {
		close(dgate_sock_);
		dgate_sock_ = -1;
	}

	ev_dgate_readable_.stop();

	do_cleanup();
}

void client::setup()
{
	// Listen on UNIX socket
	dgate_sock_ = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	int error = errno;

	if (dgate_sock_ == -1) {
		std::cerr << "dlink: socket(): could not create UNIX socket: ";
		std::cerr << strerror(error) << std::endl;
		cleanup();
		return;
	}

	struct sockaddr_un name;
	std::memset(&name, 0, sizeof(sockaddr_un));
	name.sun_family = AF_UNIX;
	std::memcpy(name.sun_path, "dgate.sock", 11);

	error = connect(dgate_sock_, (sockaddr*)&name, sizeof(sockaddr_un));
	if (error) {
		error = errno;
		std::cerr << "dlink: connect(): failed: ";
		std::cerr << strerror(error) << std::endl;
		cleanup();
		return;
	}

	fcntl(dgate_sock_, F_SETFL, O_NONBLOCK);
	ev_dgate_readable_.start(dgate_sock_, ev::READ);
	
	do_setup();
}

void client::do_setup() {}
void client::do_cleanup() {}

void client::dgate_readable(ev::io&, int)  {
	dgate::packet p;

	int count = read(dgate_sock_, &p, sizeof(dgate::packet));
	int error = errno;

	if (count == -1) {
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "client: dgate_readable: read() returned EAGAIN??" << std::endl;
			return;
		}
		std::cerr << "client: dgate_readable: read() error: ";
		std::cerr << strerror(error) << std::endl;
		// TODO: cleanup
		return;
	}

	if (count == dgate::packet_voice_size) return dgate_handle_voice(p, count);
	if (count == dgate::packet_voice_end_size) return dgate_handle_voice_end(p, count);
	if (count == dgate::packet_header_size) return dgate_handle_header(p, count);
}

void client::dgate_reply(const dgate::packet&p, size_t len) {
	ssize_t count = write(dgate_sock_, &p, len);
	int error = errno;

	if (count == -1) {
		if (error == EAGAIN || error == EWOULDBLOCK) {
			std::cerr << "client: dgate_reply(): write() returned EAGAIN!" << std::endl;
			// TODO: how common is this? do we need a send queue?
			return;
		}
		std::cerr << "client: dgate_reply(): write(): error ";
		std::cerr << std::strerror(error) << std::endl;
		cleanup();
		return;
	}
	if (count == 0) {
		std::cerr << "client: dgate_reply(): write(): 0 bytes, closing" << std::endl;
		cleanup();
		return;
	}
}

void client::run() {
	loop_.run();
}

client::~client() {
	cleanup();
}

}// namespace dgate
