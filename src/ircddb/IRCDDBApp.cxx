#include "IRCDDBApp.h"
#include <algorithm>
#include <cctype>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

IRCDDBApp::IRCDDBApp(const std::string& cs, const std::vector<IRCClientConfig>& configs, const std::string& database)
	: done(false), error(false), loop_(), evMsgEnqueued_(loop_)
{
	callsign_ = str_tolower(cs);
	// TODO: verify realname field
	std::string realname = ":CIRCDDB: dgate 0.0.1";

	for (const auto& c : configs) {
		auto store = std::make_shared<IRCClientStore>();

		store->watcher = std::make_shared<ev::async>(loop_);
		store->watcher->set<IRCDDBApp, &IRCDDBApp::msgReceived>(this);
		store->watcher->start();

		store->client = std::make_unique<IRCClient>(c.host, c.port, c.pass, callsign_ + "-1", callsign_, "CIRCDDB:2.0.0 d-gate0001", store->watcher);
		store->cfg = c;

		clients_.push_back(std::move(store));
	}

	evMsgEnqueued_.set<IRCDDBApp, &IRCDDBApp::msgEnqueued>(this);
	evMsgEnqueued_.start();
}

// This should run on a separate thread
void IRCDDBApp::run()
{
	done = false;

	std::vector<std::future<void>> futures;

	for (const auto& c : clients_) {
		auto future = std::async(std::launch::async, [=]() {
			if(!c->client->connect())
				c->client->run();
		});
		futures.push_back(std::move(future));
	}

	loop_.run();
}

void IRCDDBApp::enqueueMessage(const IRCMessage& msg) {
	msgQueue_.push(msg);
	evMsgEnqueued_.send();
}

void IRCDDBApp::msgEnqueued(ev::async&, int) {
	// Use the prefix to determine which server to send to.
	// Normally we don't wanna send outgoing prefixes to the server.
	while (auto m = msgQueue_.pop()) {
		auto msg = *m;
		if (msg.prefix) {
			int i;
			try {
				i = std::stoi(*msg.prefix);
			} catch (std::invalid_argument const&) {
				std::cerr << "BUG: invalid IRC message destination" << std::endl;
				return; // We really shouldn't be seeing this.
			} catch (std::out_of_range const&) {
				std::cerr << "BUG: invalid IRC message destination" << std::endl;
				return;
			}
			msg.prefix = {};

			if (0 <= i && i < (int)clients_.size()) {
				clients_[i]->client->dispatchCommand(msg);
			}
		} else {
			for (std::vector<IRCClient>::size_type i = 0; i < clients_.size(); i++) {
				clients_[i]->client->dispatchCommand(msg);
			}
		}
	}
}

void IRCDDBApp::msgReceived(ev::async& watcher, int) {
	for (std::vector<IRCClientStore>::size_type i = 0; i < clients_.size(); i++) {
		if (clients_[i]->watcher.get() == &watcher) {
			std::cout << "msg from " << i << ": ";
			while (auto msg = clients_[i]->client->recvMsgQueue.pop()) {
				std::cout << *msg;
			}
		}
	}
}
