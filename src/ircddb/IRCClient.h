#ifndef _IRCClient_h
#define _IRCClient_h
#include "common/TQueue.h"
#include "ircddb/IRCMessage.h"
#include <cstdint>
#include <ev++.h>
#include <memory>
#include <sstream>
#include <string>

enum IRCClientState {
	CLOSED,
	ERRORED,
	AUTH_PASS,
	AUTH_NICK,
	AUTH_USER,
	CONNECTED
};

class IRCClient {
public:
	IRCClient(const std::string& host, const uint_least16_t port, const std::string& pass, const std::string& nick, const std::string& user, const std::string& realname, std::shared_ptr<ev::async> evCallback);

	// Resolves the host and opens a connection socket, but doesn't
	// connect.
	int setup();

	// Begin connection, setup event handler.
	int connect();

	void setHost(const std::string& host);
	void setPort(const uint_least16_t& port);
	void setPass(const std::string& pass);
	void setNick(const std::string& nick);
	void setUser(const std::string& user);
	void setRealname(const std::string& realname);

	void dispatchCommand(const IRCMessage& msg);

	IRCClientState state;
	TQueue<IRCMessage> recvMsgQueue;

	void run();

private:
	int sendCommand(const IRCMessage& msg);
	int sendCommand(const std::string& msg);

	void readable(ev::io& io, int revents);
	void writeable(ev::io& io, int revents);
	void timeout(ev::timer& timer, int revents);
	void msgInQueue(ev::async&, int revents);
	void cleanup();

	void processInMessage(const IRCMessage& msg);

	TQueue<IRCMessage> sendMsgQueue_;

	ev::dynamic_loop loop_;

	ev::io evNewData_;
	ev::io evWriteable_;
	ev::timer evTimeout_;
	ev::async evMsgInQueue_;

	std::shared_ptr<ev::async> evNewMessageCallback_;

	std::string host_;
	uint_least16_t port_;
	std::string pass_;
	std::string nick_;
	std::string user_;
	std::string realname_;
	int socketFd_;

	std::stringstream outBuffer_;
	std::stringstream inBuffer_;

};

#endif
