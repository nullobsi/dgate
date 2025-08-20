//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
//
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
//   3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "stream.h"
#include "dv/types.h"
#include <cstring>

#define MSG 0
#define SERIAL 1
namespace dv {
void stream::prepare()
{
	int seqno = 0;
	bool msg_sent = false;
	bool serial_sent = false;

	int msg_idx = 0;
	std::string::size_type serial_idx = 0;

	std::vector<dv::rf_frame>::size_type min_frames = 0;

	if (!tx_msg.empty()) {
		tx_msg.resize(20, ' ');
		// The tx message requires at least 4 segments.
		min_frames += 8;
	}
	else {
		msg_sent = true;
	}

	if (!serial_data.empty()) {
		// Every 5 bytes of data needs a segment.
		// Round up in this case.
		min_frames += ((serial_data.size() + (5 - 1)) / 5) * 2;
	}
	else {
		serial_sent = true;
	}

	// Every first frame is a sync frame, and thus is unusable.
	int num_syncs = ((min_frames + (20 - 1)) / 20);
	if (d_sql) {
		// The code squelch takes up another segment right after the
		// sync frame.
		min_frames += num_syncs * 2;
	}
	min_frames += num_syncs;

	// Get rid of ending frames (we will add ourselves.)
	if (frames.size() > 0 && frames[frames.size() - 1].is_end()) frames.pop_back();
	std::optional<dv::rf_frame> preend_voice = {};

	// Excluding the sync frame, for these transmissions, we'd like the
	// length to be a multiple of two for the segments.
	auto cur_size = frames.size();
	if (cur_size > min_frames) {
		cur_size = cur_size % 21;
		if (cur_size > 0) cur_size = cur_size - 1;
		// Uneven number!
		if (cur_size % 2 == 1) {
			// Store this voice data for retransmission.
			preend_voice = frames[frames.size() - 1];
			frames.pop_back();
		}
	}
	else if (cur_size < min_frames) {
		rf_frame f;
		std::memcpy(&f.ambe, dv::rf_ambe_null, sizeof(f.ambe));
		// Add null frames until the minimum is reached.
		frames.resize(min_frames, f);
	}

	int previous = 1;// 0 is for message, 1 is for serial

	uint8_t buf[3];
	std::vector<dv::rf_frame>::size_type i;
	for (i = 0; i < frames.size(); i++) {
		if (seqno == 0) {
			std::memcpy(frames[i].data, rf_data_sync, 3);
		}

		else if (d_sql && seqno == 1) {
			buf[0] = F_DSQL | 0x02U;
			buf[1] = *d_sql & 0xFFU;
			buf[2] = (*d_sql >> 8) & 0xFFU;
			dv::scram_data(frames[i].data, buf);
		}

		else if (d_sql && seqno == 2) {
			std::memcpy(frames[i].data, rf_data_null, 3);
		}

		else if (seqno % 2 == 1) {
			if (!serial_sent && (msg_sent || previous == MSG)) {
				uint8_t tosend = std::min<std::string::size_type>(serial_data.size() - serial_idx, 5);
				buf[0] = F_DATA | (tosend & 0x0FU);
				buf[1] = serial_data[serial_idx];
				buf[2] = (tosend > 1 ? serial_data[serial_idx + 1] : 0x66U);
				dv::scram_data(frames[i].data, buf);
			}
			else if (!tx_msg.empty() && (serial_sent || previous == SERIAL)) {
				buf[0] = F_TXMSG | msg_idx;
				buf[1] = tx_msg[msg_idx * 5];
				buf[2] = tx_msg[msg_idx * 5 + 1];
				dv::scram_data(frames[i].data, buf);
			}
		}
		else {
			if (!serial_sent && (msg_sent || previous == MSG)) {
				uint8_t tosend = std::min<std::string::size_type>(serial_data.size() - serial_idx, 5);
				buf[0] = (tosend > 2 ? serial_data[serial_idx + 2] : 0x66U);
				buf[1] = (tosend > 3 ? serial_data[serial_idx + 3] : 0x66U);
				buf[2] = (tosend > 4 ? serial_data[serial_idx + 4] : 0x66U);
				dv::scram_data(frames[i].data, buf);
				serial_idx += tosend;
				if (serial_idx >= serial_data.size()) serial_sent = true;
				previous = SERIAL;
			}
			else if (!tx_msg.empty() && (serial_sent || previous == SERIAL)) {
				buf[0] = tx_msg[msg_idx * 5 + 2];
				buf[1] = tx_msg[msg_idx * 5 + 3];
				buf[2] = tx_msg[msg_idx * 5 + 4];
				dv::scram_data(frames[i].data, buf);
				msg_idx++;
				if (msg_idx == 4) {
					msg_sent = true;
					msg_idx = 0;
				}
				previous = MSG;
			}
		}

		seqno = (seqno + 1) % 21;
	}

	dv::rf_frame f;
	// Add ending frames
	std::memcpy(f.data, dv::rf_data_preend, 3);
	if (preend_voice) {
		std::memcpy(f.ambe, preend_voice->ambe, 9);
		frames.push_back(f);
	}
	else if (frames[frames.size() - 1].is_sync()) {
		std::memcpy(frames[frames.size() - 1].data, dv::rf_data_preend, 3);
	}
	else {
		std::memcpy(f.ambe, dv::rf_ambe_null, 9);
		frames.push_back(f);
	}

	std::memcpy(f.ambe, dv::rf_ambe_end, 9);
	f.data[0] = 0;
	f.data[1] = 0;
	f.data[2] = 0;
	frames.push_back(f);

	// Done!
}

}// namespace dv
