#ifndef DV_FRAME_H
#define DV_FRAME_H
#include <cstdint>

namespace dv {

enum miniheader_flags {
	F_DATA = 0x30,
	F_TXMSG = 0x40,
	F_HEADER = 0x50,
	F_FASTDATA_1 = 0x80,
	F_FASTDATA_2 = 0x90,
	F_DSQL = 0xC0,
	F_EMPTY = 0x66,
};

struct frame;

// Encoded D-STAR voice + data frame. This is exactly what would get
// sent over RF.
#pragma pack(push, 1)
struct rf_frame {
	uint8_t ambe[9];// Proprietary AMBE data with FEC.
	uint8_t data[3];// Scrambled data.

	bool is_sync() const;
	bool is_preend() const;
	bool is_end() const;
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
	bool is_preend() const;
	bool is_end() const;
	rf_frame encode() const;
};

static constexpr uint8_t rf_ambe_null[9] = {0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U};
static constexpr uint8_t rf_data_scram[3] = {0x70U, 0x4FU, 0x93U};
static constexpr uint8_t rf_data_null[3] = {F_EMPTY ^ 0x70U, F_EMPTY ^ 0x4FU, F_EMPTY ^ 0x93U};
static constexpr uint8_t rf_data_sync[3] = {0x55U, 0x2DU, 0x16U};
static constexpr uint8_t rf_data_preend[3] = {0x55U, 0x55U, 0x55U};
static constexpr uint8_t rf_ambe_end[9] = {0x55, 0xC8, 0x7A, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};

static inline constexpr void scram_data(uint8_t out[3], const uint8_t in[3])
{
	out[0] = rf_data_scram[0] ^ in[0];
	out[1] = rf_data_scram[1] ^ in[1];
	out[2] = rf_data_scram[2] ^ in[2];
}

}// namespace dv

#endif
