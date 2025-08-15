#include "client.h"
#include "ircddb/client.h"
#include "ircddb/irc_msg.h"
#include <cerrno>
#include <cstring>
#include <ev++.h>
#include <fcntl.h>
#include <ios>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>

namespace ircddb {

client::client(const std::string& host, const uint_least16_t port, const std::string& pass, const std::string& nick, const std::string& user, const std::string& realname, std::shared_ptr<ev::async> ev_msg_in_callback)
	: loop_(), ev_sock_readable_(loop_), ev_sock_writable_(loop_), ev_sock_timeout_(loop_), ev_msg_out_(loop_), ev_msg_in_callback_(ev_msg_in_callback), host_(host), port_(port), pass_(pass), nick_(nick), user_(user), realname_(realname)
{
	ev_sock_readable_.set<client, &client::readable>(this);
	ev_sock_writable_.set<client, &client::writable>(this);
	ev_sock_timeout_.set<client, &client::timeout>(this);
	ev_msg_out_.set<client, &client::msg_out>(this);
}

void client::run()
{
	loop_.run();
};

int client::connect()
{
	int status;
	struct addrinfo hints;
	struct addrinfo* servinfo = nullptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	status = getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &servinfo);
	if (status) {
		std::cerr << "gai error: " << gai_strerror(status) << std::endl;
		if (servinfo != nullptr)
			freeaddrinfo(servinfo);
		return status;
	}

	struct addrinfo* res;
	int errn = 0;
	for (res = servinfo; res != nullptr; res = res->ai_next) {
		socketFd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		if (socketFd_ == -1) {
			errn = errno;
			continue;
		}

		if (::connect(socketFd_, res->ai_addr, res->ai_addrlen) != -1) break;

		errn = errno;
		close(socketFd_);
	}
	freeaddrinfo(servinfo);

	if (res == nullptr) {
		std::cerr << "IRCClient: could not connect to " << host_ << " on port " << port_ << ": ";
		std::cerr << strerror(errn) << std::endl;
		return errn;
	}

	fcntl(socketFd_, F_SETFL, O_NONBLOCK);
	ev_sock_writable_.set(socketFd_, ev::WRITE);
	ev_sock_readable_.set(socketFd_, ev::READ);

	ev_sock_readable_.start();
	ev_sock_timeout_.start(45., 45.);
	ev_msg_out_.start();

	send_msg("PASS :" + pass_ + "\r\n");
	send_msg("NICK :" + nick_ + "\r\n");
	send_msg("USER " + user_ + " 0 * :" + realname_ + "\r\n");

	return 0;
}

void client::cleanup()
{
	if (socketFd_ != -1) {
		if (state != ERRORED) write(socketFd_, "QUIT\r\n", 6);
		close(socketFd_);
		socketFd_ = -1;
	}
	ev_sock_readable_.stop();
	ev_sock_writable_.stop();
	ev_sock_timeout_.stop();
	ev_msg_out_.stop();
	outBuffer_ = std::stringstream();
	inBuffer_ = std::stringstream();
	while (queue_msg_in.pop()) {}
	if (state != ERRORED) state = CLOSED;
}

void client::queue_msg(const irc_msg& msg)
{
	if (msg.command == IRCMESSAGE_INVALID) return;

	queue_msg_out_.push(msg);
	ev_msg_out_.send();
}

int client::send_msg(const std::string& raw)
{
	auto count = write(socketFd_, raw.c_str(), raw.size());
	if (count == -1) {
		auto errn = errno;
		if (errn == EAGAIN || errn == EWOULDBLOCK) {
			outBuffer_ << raw;
			ev_sock_writable_.start();
			return 0;
		}
		else {
			std::cerr << "IRCClient " << host_ << ":" << port_ << " write error: ";
			std::cerr << strerror(errn) << std::endl;
			state = ERRORED;
			cleanup();
			return errn;
		}
	}
	else if ((size_t)count < raw.size()) {
		// Leftovers will be queued for writing.
		size_t leftover = raw.size() - count;
		outBuffer_.write(raw.c_str() + count, leftover);
		ev_sock_writable_.start();
	}
	return 0;
}

void client::writable(ev::io&, int)
{
	// Check outBuffer and resend bytes as needed.
	auto toWrite = outBuffer_.rdbuf()->in_avail();
	if (toWrite <= 0) {
		// well, what are we doing here???
		outBuffer_ = std::stringstream();
		ev_sock_writable_.stop();
		return;
	}

	auto count = write(socketFd_, outBuffer_.view().data(), toWrite);
	if (count == -1) {
		auto errn = errno;
		if (errn == EAGAIN || errn == EWOULDBLOCK) {
			std::cerr << "BUG: IRCClient::writeable called but write() returned EAGAIN!" << std::endl;
			return;
		}
		else {
			std::cerr << "IRCClient " << host_ << ":" << port_ << " write error: ";
			std::cerr << strerror(errn) << std::endl;
			state = ERRORED;
			cleanup();
			return;
		}
	}
	else if (count < toWrite) {
		size_t leftover = toWrite - count;
		outBuffer_.seekg(leftover, std::ios_base::cur);
	}
	else {
		outBuffer_ = std::stringstream();
	}
}

void client::timeout(ev::timer&, int)
{
	// If we're here, we have not recieved data from the server
	// for 45 seconds.

	std::cerr << "IRCClient " << host_ << ":" << port_ << " timeout." << std::endl;
	state = ERRORED;
	cleanup();
}

// An IRC message should be no more than 512
// characters, so this should be plenty for now.
#define IRCMSG_BUF 544

void client::readable(ev::io&, int)
{
	char buf[IRCMSG_BUF];

	auto count = read(socketFd_, buf, IRCMSG_BUF);
	while (count > 0 && count <= IRCMSG_BUF) {
		inBuffer_.write(buf, count);
		count = read(socketFd_, buf, IRCMSG_BUF);
	}

	if (count == -1) {
		auto errn = errno;
		if (errn != EAGAIN && errn != EWOULDBLOCK) {
			std::cerr << "IRCClient " << host_ << ":" << port_ << " read error: ";
			std::cerr << strerror(errn) << std::endl;
			state = ERRORED;
			cleanup();
			return;
		}
	}
	else if (count == 0) {
		std::cerr << "IRCClient " << host_ << ":" << port_ << " closed connection.";
		state = ERRORED;
		cleanup();
		return;
	}

	ev_sock_timeout_.again();

	auto view = inBuffer_.view();

	std::string::size_type from = 0;
	std::string::size_type to = view.find('\n');
	while (to != std::string::npos) {
		auto msg = std::string(view.substr(from, to - from + 1));
		if (msg_in(msg)) {
			std::cout << "ircclient invalid: " << msg;
		}
		from = to + 1;
		to = view.find('\n', from);
	}

	if (view.substr(from).empty()) {
		inBuffer_ = std::stringstream();
	}
	else {
		inBuffer_ = std::stringstream(std::string(view.substr(from)), std::ios_base::in | std::ios_base::out | std::ios_base::ate);
	}

	if (queue_msg_in.size() != 0)
		ev_msg_in_callback_->send();
}

void client::msg_out(ev::async&, int)
{
	while (auto msg = queue_msg_out_.pop()) {
		if ((*msg).command == IRCMESSAGE_INVALID) continue;
		std::cout << "Send message: " << *msg;
		if (msg->command == "QUIT") {
			cleanup();
			state = client_state::CLOSED;
		}
		else {
			send_msg(msg->compose());
		}
	}
}

int client::msg_in(const irc_msg& msg)
{
	// XXX: this is debug
	if (msg.command == IRCMESSAGE_INVALID) {
		return 1;
	}

	// TODO: code parsing not implemented yet
	if (msg.code) {
		switch (*msg.code) {
			// Certain messages we can discard for now.
		}
		queue_msg_in.push(msg);
	}
	else {
		if (msg.command == "PING") {
			if (msg.params && msg.params->trailer) {
				queue_msg_out_.push({"PONG", {*msg.params->trailer}, {}});
				ev_msg_out_.send();
			}
		}
		else
			queue_msg_in.push(msg);
	}
	return 0;
}

}// namespace ircddb
