#ifndef DV_HEADER_H
#define DV_HEADER_H

#include <cstdint>
namespace dv {

enum header_flags_a {
	H_DATA = 0x10000000,
	H_REPEATER = 0x01000000,
	H_INTERRUPT = 0x00100000,
	H_CONTROL = 0x00010000,
	H_URGENT = 0x00001000,

	// These flags are compound. Discard the top bits and compare the
	// least significant three.
	H_RPTR_CTRL = 0x111,
	H_AUTO_REPLY = 0x110,
	H_RESEND = 0x100,
	H_ACK = 0x011,
	H_NO_REPLY = 0x010,
	H_RELAY_UNAVAIL = 0x001,
};

enum miniheader_flags {
	F_DATA = 0x30,
	F_TXMSG = 0x40,
	F_HEADER = 0x50,
	F_FASTDATA_1 = 0x80,
	F_FASTDATA_2 = 0x90,
	F_CSQL = 0xC0,
	F_EMPTY = 0x66,
};

struct header;

// This is the 660 bit header as sent over RF.
#pragma pack(push, 1)
struct rf_header {
	// This should technically be 82.5 bytes.
	uint8_t data[83];

	header decode() const;
};
#pragma pack(pop)

// Decoded D-STAR header.
#pragma pack(push, 1)
struct header {
	// Only flag 0 (A) is used. The other two are reserved in the d-star
	// specification.
	uint8_t flags[3];

	char destination_rptr_cs[8];
	char departure_rptr_cs[8];
	char companion_cs[8];
	char own_cs[8];
	char own_cs_ext[4];

	// This value is transferred as a little-endian value.
	uint8_t crc_ccitt[2];

	uint16_t calc_crc() const;
	void set_crc(uint16_t crc);
	uint16_t get_crc() const;

	bool verify() const;

	rf_header encode() const;
};
#pragma pack(pop)

struct frame;

// Encoded D-STAR voice + data frame. This is exactly what would get
// sent over RF.
#pragma pack(push, 1)
struct rf_frame {
	uint8_t ambe[9];// Proprietary AMBE data with FEC.
	uint8_t data[3];// Scrambled data.

	bool is_sync() const;
	frame decode() const;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct rf_frame_end {
	rf_frame f;
	uint8_t end_seq[3];// Extra data sent at the end of a transmission.
};
#pragma pack(pop)

// Since we can't really decode AMBE data to voice, this is just rf_frame but
// with the data field unscrambled and the AMBE data decoded (error
// correction applied).
struct frame {
	// These are technically 12-bit fields.
	uint16_t ambe_decode_1;
	uint16_t ambe_decode_2;
	uint8_t ambe[3];// The remaining untouched AMBE data.
	char data[3];   // Unscrambled data.

	uint8_t bit_errors;// Number of bit errors from the process of decoding AMBE data.

	bool is_sync() const;
	rf_frame encode() const;
};

static constexpr uint8_t rf_ambe_null[9] = {0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U};
static constexpr uint8_t rf_data_null[3] = {0x70U, 0x4FU, 0x93U};
static constexpr uint8_t rf_data_sync[3] = {0x55U, 0x2DU, 0x16U};
static constexpr uint8_t rf_data_end[3] = {0x55, 0xC8, 0x7A};

}// namespace dv

#endif
