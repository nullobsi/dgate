#ifndef DGATE_CLIENT_H
#define DGATE_CLIENT_H

#include "dgate/dgate.h"
#include <ev++.h>
#include <string>
namespace dgate {

class client {
public:
	client(const std::string& dgate_socket_path, const std::string& cs);

	void setup();
	void cleanup();
	virtual void run();

	virtual ~client();

protected:
	virtual void do_setup();
	virtual void do_cleanup();

	virtual void dgate_handle_header(const packet& p, size_t len) = 0;
	virtual void dgate_handle_voice(const packet& p, size_t len) = 0;
	virtual void dgate_handle_voice_end(const packet& p, size_t len) = 0;

	void dgate_reply(const dgate::packet&p, size_t len);

	std::string cs_;

	ev::dynamic_loop loop_;

	int dgate_sock_;

private:
	void dgate_readable(ev::io&, int);
	ev::io ev_dgate_readable_;
};

}// namespace dgate

#endif
