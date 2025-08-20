#ifndef DV_STREAM_H
#define DV_STREAM_H

#include "frame.h"
#include "header.h"
#include <ev++.h>
#include <optional>
#include <vector>
namespace dv {

struct stream {
	dv::header header;
	std::vector<dv::rf_frame> frames;

	std::string tx_msg;
	std::string serial_data;

	std::optional<uint16_t> d_sql;// digital squelch

	void prepare();
	void prepare_async(ev::async* cb);
};

}// namespace dv

#endif
