#include "IRCMessage.h"
#include <iostream>
#include <regex>
#include <sstream>

std::ostream& operator<<(std::ostream& os, const IRCMessage& msg)
{
	if (msg.prefix) {
		os << ":" << *msg.prefix << " ";
	}
	os << msg.command;
	if (msg.params) {
		const auto& p = *msg.params;

		for (const auto& param : p.list) {
			os << " " << param;
		}

		if (p.trailer) {
			// okay, technically this is spec, but i don't wanna do it
			/*if (p.list.size() == 14) {
				os << " " << *p.trailer;
			} else {*/
			os << " :" << *p.trailer;
			//}
		}
	}
	os << "\r\n";
	return os;
}

std::string IRCMessage::compose() const
{
	std::stringstream s;
	s << this;
	return s.str();
}

static const std::regex IRC_MSG_REGEX("^(?:\\:([A-Za-z0-9\\-.]+|[A-Za-z0-9\\-\\x5B-\\x60\\x7B-\\x7D]+(?:(?:![^\\0\\r\\n @]+)?@[A-Za-z0-9\\-.:]+)?) )?([A-Za-z]+|[0-9]{3})((?: [^\\0\\r\\n :]+){0,14})( :?[^\\0\\r\\n]*?)?\\r?\\n?$", std::regex_constants::ECMAScript | std::regex_constants::optimize);

IRCMessage::IRCMessage(const std::string& raw)
{
	std::smatch base_match;
	if (!std::regex_match(raw, base_match, IRC_MSG_REGEX, std::regex_constants::match_not_null)) {
		command = IRCMESSAGE_INVALID;
		return;
	}
	if (base_match[1].matched) {
		prefix = base_match[1];
	}
	// Really, this should never happen.
	if (!base_match[2].matched) {
		command = IRCMESSAGE_INVALID;
		return;
	}
	command = base_match[2];

	auto p = IRCParams();
	if (base_match[3].matched) {
		std::istringstream buf(base_match[3]);
		std::string arg;
		while (buf >> arg) {
			p.list.push_back(arg);
		}
	}
	if (base_match[4].matched) {
		auto trailer = base_match[4].str();
		if (trailer.starts_with(' ')) trailer = trailer.substr(1);
		if (trailer.starts_with(':')) trailer = trailer.substr(1);
		p.trailer = trailer;
	}
	if (base_match[3].matched || base_match[4].matched) params = p;
}

IRCMessage::IRCMessage(const std::string& to, const std::string& msg)
{
	command = "PRIVMSG";

	auto p = IRCParams();
	p.list.push_back(to);
	p.trailer = msg;
	params = p;
}

IRCMessage::IRCMessage(const std::string& command_, const std::vector<std::string>& params_, const std::optional<std::string>& trailer_)
{
	command = command_;
	auto p = IRCParams();
	if (!params_.empty()) {
		p.list = params_;
		params = p;
	}
	if (trailer_) {
		p.trailer = *trailer_;
		params = p;
	}
}
