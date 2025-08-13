#ifndef _IRCDDBApp_h
#define _IRCDDBApp_h
#include "client.h"
#include "common/TQueue.h"
#include "common/lmdb++.h"
#include "irc_msg.h"
#include <atomic>
#include <cstdint>
#include <ev++.h>
#include <map>
#include <memory>
#include <sys/socket.h>
#include <vector>

namespace ircddb {

struct client_cfg {
	std::string host;
	std::string pass;
	std::string update_channel;
	uint_least16_t port;
	uint_least16_t af;// address family of IPs on the irc server
};

struct client_store {
	client_cfg cfg;
	std::shared_ptr<ev::async> watcher;
	std::unique_ptr<client> client;
	std::map<std::string, std::string> gate_nick;
	std::string server_nick;
	std::string current_nick;
};

class app {
public:
	app(const std::string& callsign, const std::vector<client_cfg>& configs,
	    std::shared_ptr<lmdb::env> env,
	    std::shared_ptr<lmdb::dbi> cs_rptr,
	    std::shared_ptr<lmdb::dbi> zone_ip4,
	    std::shared_ptr<lmdb::dbi> zone_ip6,
	    std::shared_ptr<lmdb::dbi> zone_nick);
	void run();

	std::atomic_bool done;
	// Set to true if an error has occured.
	bool error;

	void queue_msg(const irc_msg& msg);

private:
	void msg_out(ev::async& w, int revents);
	void msg_in(ev::async& w, int revents);

	void handle_msg(int id, const irc_msg& msg);

	void handle_NAMREPLY(int id, const irc_msg& msg);
	void handle_WHOREPLY(int id, const irc_msg& msg);
	void handle_JOIN(int id, const irc_msg& msg);
	void handle_QUIT(int id, const irc_msg& msg);

	void update_gate(int id, const std::string& nick, const std::string& user, const std::string& host);
	void delete_gate(int id, const std::string& user);

	void get_all_gates(int id);

	std::string callsign_;
	std::vector<std::shared_ptr<client_store>> clients_;

	ev::dynamic_loop loop_;

	// Fired when message added to queue.
	ev::async ev_msg_out;
	TQueue<irc_msg> queue_msg_out;

	// Used for caching heard callsigns.
	std::shared_ptr<lmdb::env> env_;
	std::shared_ptr<lmdb::dbi> cs_rptr_;
	std::shared_ptr<lmdb::dbi> zone_ip4_;
	std::shared_ptr<lmdb::dbi> zone_ip6_;
	std::shared_ptr<lmdb::dbi> zone_nick_;
};

}// namespace ircddb

#endif
