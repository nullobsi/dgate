#ifndef _IRCMessage_h
#define _IRCMessage_h

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#define IRCMESSAGE_INVALID "IRCMESSAGE_INVALID"

struct IRCParams {
	std::vector<std::string> list;
	std::optional<std::string> trailer;
};

struct IRCMessage {
	IRCMessage() = default;
	IRCMessage(const std::string& raw);
	IRCMessage(const std::string& to, const std::string& msg);
	IRCMessage(const std::string& command, const std::vector<std::string>& params, const std::optional<std::string>& trailer = {});

	std::optional<std::string> prefix;
	std::string command;
	std::optional<IRCParams> params;

	std::optional<uint_least16_t> code;
	
	friend std::ostream& operator<<(std::ostream& os, const IRCMessage& msg);
	std::string compose() const;
};

#endif
