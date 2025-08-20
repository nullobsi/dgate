#ifndef ITAP_APP_H
#define ITAP_APP_H

#include "dgate/client.h"
#include <atomic>
#include <queue>
#include <random>
namespace itap {

class app : public dgate::client {
public:
	app(const std::string& dgate_socket_path, const std::string& itap_tty_path, const std::string& cs, char module);

	void run() override;

protected:
	void do_setup() override;
	void do_cleanup() override;

	void dgate_handle_header(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice(const dgate::packet& p, size_t len) override;
	void dgate_handle_voice_end(const dgate::packet& p, size_t len) override;

private:
	void itap_readable(ev::io&, int);
	void itap_ping(ev::timer&, int);
	void itap_timeout(ev::timer&, int);

	void send_from_queue();

	inline ssize_t itap_read(void* buf, size_t len);
	inline void itap_reply(const void* buf, size_t len);

	std::string itap_tty_path_;

	uint8_t msg_length_;
	uint8_t msg_ptr_;
	uint8_t buf[255];

	char module_;
	uint16_t tx_id_;
	std::atomic_flag tx_lock;

	int itap_sock_;
	ev::io ev_itap_readable_;
	ev::timer ev_itap_ping_;
	ev::timer ev_itap_timeout_;

	std::minstd_rand rand_gen_;
	std::uniform_int_distribution<uint16_t> rand_dist_;

	bool acked_;
	std::queue<dgate::packet> queue_in_;
};

}// namespace itap

#endif
