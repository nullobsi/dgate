#ifndef DGATE_APP_H
#define DGATE_APP_H

#include "dgate/dgate.h"
#include "dgate/g2.h"
#include "ircddb/app.h"
#include <ev++.h>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <unordered_set>
namespace dgate {

struct client_connection {
	int fd;
	std::unique_ptr<ev::io> watcher;
};

struct tx_state {
	char serial_buffer[512];// used to decode D-PRS data
	char tx_msg[21];        // might as well null-terminate this
	dv::header header;
	sockaddr_storage from;
	uint32_t count;     // Total count of packets
	uint8_t seqno;      // D-star frame count (mod 21)
	uint32_t bit_errors;// Number of bit errors detected
	uint16_t tx_id;
	uint8_t miniheader;
	int serial_pointer;
	bool local;

	void reset();
};

class app;
struct module {
	// This is probably bad but I'm SO TIRED.
	app* parent;
	void operator()(ev::timer&, int);

	char name;
	tx_state state;
	std::shared_ptr<ev::timer> timeout;
	mutable std::atomic_flag tx_lock;
};

class app {
	friend module;

public:
	app(std::string cs, std::unordered_set<char> modules, std::unique_ptr<ircddb::app> ircddb,
	    std::shared_ptr<lmdb::env> env, std::shared_ptr<lmdb::dbi> cs_rptr,
	    std::shared_ptr<lmdb::dbi> zone_ip4, std::shared_ptr<lmdb::dbi> zone_ip6,
	    std::shared_ptr<lmdb::dbi> zone_nick);

	void run();

private:
	void unbind_all();

	void g2_readable_v4(ev::io&, int);
	void g2_readable_v6(ev::io&, int);
	void g2_handle_packet(const g2_packet& p, size_t len, const sockaddr_storage& from);
	void g2_handle_header(const g2_packet& p, size_t len, const sockaddr_storage& from);
	void g2_handle_voice(const g2_packet& p, size_t len, const sockaddr_storage& from);

	void dgate_readable(ev::io&, int);
	void dgate_client_readable(ev::io&, int);
	void dgate_client_handle_packet(int id);

	void tx_timeout(char module);

	void handle_header(const dv::header& h, char module);
	void handle_voice(const dv::rf_frame& h, char module);
	void handle_voice_end(const dv::rf_frame& h, char module);

	void write_all_dgate(const packet& p, std::size_t len);

	ev::dynamic_loop loop_;

	std::string cs_;

	int g2_sock_v4_;
	int g2_sock_v6_;

	int dgate_sock_;

	std::vector<client_connection> dgate_conns_;

	ev::io ev_g2_readable_v4_;
	ev::io ev_g2_readable_v6_;

	ev::io ev_dgate_readable_;

	std::unordered_set<char> enabled_modules_;// Enabled modules on this GATE.
	std::unordered_map<char, std::unique_ptr<module>> modules_;
	std::unique_ptr<ircddb::app> ircddb_;

	// Memory caches for repeater information.
	std::shared_ptr<lmdb::env> env_;
	std::shared_ptr<lmdb::dbi> cs_rptr_;
	std::shared_ptr<lmdb::dbi> zone_ip4_;
	std::shared_ptr<lmdb::dbi> zone_ip6_;
	std::shared_ptr<lmdb::dbi> zone_nick_;
};

}// namespace dgate

#endif
