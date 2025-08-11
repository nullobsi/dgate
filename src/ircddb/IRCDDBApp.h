#ifndef _IRCDDBApp_h
#define _IRCDDBApp_h
#include "IRCClient.h"
#include "common/TQueue.h"
#include <atomic>
#include <cstdint>
#include <ev++.h>
#include <memory>
#include <sys/socket.h>
#include <vector>
#include "IRCMessage.h"
#include "common/lmdb++.h"

struct IRCClientConfig {
	std::string host;
	std::string pass;
	uint_least16_t port;
	uint_least16_t af;// address family of IPs on the irc server
};

struct IRCClientStore {
	IRCClientConfig cfg;
	std::shared_ptr<ev::async> watcher;
	std::unique_ptr<IRCClient> client;
};

class IRCDDBApp {
public:
	IRCDDBApp(const std::string& callsign, const std::vector<IRCClientConfig>& configs, const std::string& database);
	void run();

	std::atomic_bool done;
	// Set to true if an error has occured.
	bool error;

	void enqueueMessage(const IRCMessage& msg);


private:
	void msgEnqueued(ev::async &w, int revents);
	void msgReceived(ev::async &w, int revents);

	std::string callsign_;
	std::vector<std::shared_ptr<IRCClientStore>> clients_;

	ev::dynamic_loop loop_;

	// Fired when message added to queue.
	ev::async evMsgEnqueued_;
	TQueue<IRCMessage> msgQueue_;

	// Used for caching heard callsigns.
	std::shared_ptr<lmdb::env> env_;
	std::shared_ptr<lmdb::dbi> cs_rptr_dbi_;
	std::shared_ptr<lmdb::dbi> zone_ip4_;
	std::shared_ptr<lmdb::dbi> zone_ip6_;
};

#endif
