#include "IRCClient.h"
#include "ircddb/IRCMessage.h"
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

IRCClient::IRCClient(const std::string& host, const uint_least16_t port, const std::string& pass, const std::string& nick, const std::string& user, const std::string& realname, std::shared_ptr<ev::async> evCallback)
	: loop_(), evNewData_(loop_), evWriteable_(loop_), evTimeout_(loop_), evMsgInQueue_(loop_), evNewMessageCallback_(evCallback), host_(host), port_(port), pass_(pass), nick_(nick), user_(user), realname_(realname)
{
	evNewData_.set<IRCClient, &IRCClient::readable>(this);
	evWriteable_.set<IRCClient, &IRCClient::writeable>(this);
	evTimeout_.set<IRCClient, &IRCClient::timeout>(this);
	evMsgInQueue_.set<IRCClient, &IRCClient::msgInQueue>(this);
}

void IRCClient::run()
{
	loop_.run();
};

int IRCClient::connect()
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
	evWriteable_.set(socketFd_, ev::WRITE);
	evNewData_.set(socketFd_, ev::READ);

	evNewData_.start();
	evTimeout_.start(45., 45.);
	evMsgInQueue_.start();

	sendCommand("PASS :" + pass_ + "\r\n");
	sendCommand("NICK :" + nick_ + "\r\n");
	sendCommand("USER " + user_ + " 0 * :" + realname_ + "\r\n");

	return 0;
}

void IRCClient::cleanup()
{
	if (socketFd_ != -1) {
		if (state != ERRORED) write(socketFd_, "QUIT\r\n", 6);
		close(socketFd_);
		socketFd_ = -1;
	}
	evNewData_.stop();
	evWriteable_.stop();
	evTimeout_.stop();
	evMsgInQueue_.stop();
	outBuffer_ = std::stringstream();
	inBuffer_ = std::stringstream();
	while (recvMsgQueue.pop()) {}
}

void IRCClient::dispatchCommand(const IRCMessage& msg)
{
	if (msg.command == IRCMESSAGE_INVALID) return;

	recvMsgQueue.push(msg);
	evMsgInQueue_.send();
}

int IRCClient::sendCommand(const std::string& raw)
{
	auto count = write(socketFd_, raw.c_str(), raw.size());
	if (count == -1) {
		auto errn = errno;
		if (errn == EAGAIN || errn == EWOULDBLOCK) {
			outBuffer_ << raw;
			evWriteable_.start();
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
		evWriteable_.start();
	}
	return 0;
}

int IRCClient::sendCommand(const IRCMessage& msg)
{
	std::string cmd = msg.compose();
	return sendCommand(cmd);
}

void IRCClient::writeable(ev::io&, int)
{
	// Check outBuffer and resend bytes as needed.
	auto toWrite = outBuffer_.rdbuf()->in_avail();
	if (toWrite <= 0) {
		// well, what are we doing here???
		outBuffer_ = std::stringstream();
		evWriteable_.stop();
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

void IRCClient::timeout(ev::timer&, int)
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

void IRCClient::readable(ev::io&, int)
{
	char buf[IRCMSG_BUF];

	auto count = read(socketFd_, &buf[0], IRCMSG_BUF);
	while (count == IRCMSG_BUF) {
		inBuffer_.write(buf, IRCMSG_BUF);
		count = read(socketFd_, &buf[0], IRCMSG_BUF);
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

	evTimeout_.again();

	auto view = inBuffer_.view();

	std::string::size_type from = 0;
	std::string::size_type to = view.find('\n');
	while (to != std::string::npos) {
		auto msg = std::string(view.substr(from, to - from + 1));
		processInMessage(msg);
		from = to + 1;
		to = view.find('\n', from);
	}

	inBuffer_ = std::stringstream(std::string(view.substr(from)));

	if (recvMsgQueue.size() != 0)
		evNewMessageCallback_->send();
}

void IRCClient::msgInQueue(ev::async&, int)
{
	while (auto msg = recvMsgQueue.pop()) {
		if ((*msg).command == IRCMESSAGE_INVALID) continue;
		sendCommand(*msg);
	}
}

void IRCClient::processInMessage(const IRCMessage& msg)
{
	// XXX: this is debug
	//std::cout << "ircclient:" << msg;
	if (msg.command == IRCMESSAGE_INVALID) return;

	// TODO: code parsing not implemented yet
	if (msg.code) {
		switch (*msg.code) {
			// Certain messages we can discard for now.
		}
	}
	else {
		recvMsgQueue.push(msg);
	}
}
