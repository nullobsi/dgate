#include "tables.h"
#include "types.h"
#include <bit>
#include <cstring>

// Thanks to QNetGateway and N7TAE for this code.
#define interleaveambe12(bp)   \
	{                          \
		bp += 12;              \
		if (bp > 71) bp -= 71; \
	}
#define GORLAY_X22 0x00400000   // vector representation of X^{22}
#define GORLAY_X11 0x00000800   // vector representation of X^{11}
#define GORLAY_MASK12 0xfffff800// auxiliary vector for testing
#define GORLAY_GENPOL 0x00000c75// generator polinomial, g(x)

namespace dv {

/*
 * Compute the syndrome corresponding to the given pattern, i.e., the
 * remainder after dividing the pattern (when considering it as the vector
 * representation of a polynomial) by the generator polynomial, GENPOL.
 * In the program this pattern has several meanings: (1) pattern = infomation
 * bits, when constructing the encoding table; (2) pattern = error pattern,
 * when constructing the decoding table; and (3) pattern = received vector, to
 * obtain its syndrome in decoding.
 */

uint16_t get_syndrome_23127(uint_fast32_t pattern)
{
	uint_fast32_t aux = GORLAY_X22;
	if (pattern >= GORLAY_X11) {
		while (pattern & GORLAY_MASK12) {
			while ((aux & pattern) == 0) aux >>= 1;
			pattern ^= (aux / GORLAY_X11) * GORLAY_GENPOL;
		}
	}
	return pattern;
}

uint16_t gorlay_decode23127(uint_fast32_t code, uint8_t& bit_errors_out)
{
	auto syndrome = get_syndrome_23127(code);
	uint32_t error_pattern = DECODING_TABLE_23127[syndrome];
	code ^= error_pattern;
	bit_errors_out += std::popcount(error_pattern);
	return (code >> 11);
}

uint32_t gorlay_encode24128(uint16_t data)
{
	return ENCODING_TABLE_24128[data];
}

uint16_t gorlay_decode24128(uint_fast32_t code, uint8_t& bit_errors_out)
{
	// Discard the parity bit, and try error correction anyway.
	return gorlay_decode23127(code >> 1, bit_errors_out);
}

void ambefec_deinterleave(uint8_t out[9], const uint8_t in[9])
{
	uint_fast32_t bitpos, bytcnt;
	memset(out, 0, 9);// init result
	bitpos = 0;
	for (bytcnt = 0; bytcnt < 9; bytcnt++) {
		uint8_t voice_dsr = in[bytcnt];
		if (voice_dsr & 0x80) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x40) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x20) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x10) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x08) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x04) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x02) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
		if (voice_dsr & 0x01) out[bitpos >> 3] |= (0x80 >> (bitpos & 7));
		interleaveambe12(bitpos);
	}
}

void ambefec_interleave(uint8_t out[9], const uint8_t in[9])
{
	uint_fast32_t bitpos, bytcnt;
	memset(out, 0, 9);// init result
	bitpos = 0;
	for (bytcnt = 0; bytcnt < 9; bytcnt++) {
		uint8_t voice_dsr = (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) ? 0x80 : 0x00;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x40;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x20;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x10;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x08;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x04;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x02;
		interleaveambe12(bitpos);
		if (in[bitpos >> 3] & (0x80 >> (bitpos & 7))) voice_dsr |= 0x01;
		interleaveambe12(bitpos);
		out[bytcnt] = voice_dsr;
	}
}

bool rf_frame::is_sync() const
{
	return memcmp(rf_data_sync, data, 3) == 0;
}

bool frame::is_sync() const
{
	return memcmp(rf_data_sync, data, 3) == 0;
}

bool rf_frame::is_preend() const {
	return memcmp(rf_data_preend, data, 3) == 0;
}

bool frame::is_preend() const {
	return memcmp(rf_data_preend, data, 3) == 0;
}

bool rf_frame::is_end() const
{
	return memcmp(rf_ambe_end, ambe, 9) == 0;
}

bool frame::is_end() const
{
	return ambe_decode_1 == 0 && ambe_decode_2 == 0 && memcmp(rf_ambe_end, ambe, 3) == 0;
}

frame rf_frame::decode() const
{
	frame f;

	// The original AMBE frames after decoding the Gorlay code.
	// They're 16 bit values, but technically only 12 bits are used.
	uint_fast16_t decoded_ambe_1;
	uint_fast16_t decoded_ambe_2;
	uint8_t bit_errors = 0;

	uint8_t deinterleaved[9];
	ambefec_deinterleave(deinterleaved, this->ambe);

	decoded_ambe_1 = gorlay_decode24128((deinterleaved[0] << 16) | (deinterleaved[1] << 8) | deinterleaved[2], bit_errors);

	uint_fast32_t prng_xor = PRNG_TABLE[decoded_ambe_1];
	decoded_ambe_2 = gorlay_decode24128(((deinterleaved[3] << 16) | (deinterleaved[4] << 8) | deinterleaved[5]) ^ prng_xor, bit_errors);

	// TODO: Find other people who store decoded AMBE frames. How do
	// they pack it after decoding?
	f.ambe_decode_1 = decoded_ambe_1;
	f.ambe_decode_2 = decoded_ambe_2;
	std::memcpy(f.ambe, &deinterleaved[6], 3);
	f.bit_errors = bit_errors;

	if (is_sync() || is_preend()) {
		// We don't decode the sync frame, because it is excluded from
		// scrambling.
		std::memcpy(f.data, this->data, 3);
	}
	else if (is_end()) {
		std::memcpy(f.data, this->data, 3);
		std::memcpy(f.ambe, rf_ambe_end, 3);
		f.ambe_decode_1 = 0;
		f.ambe_decode_2 = 0;
		f.bit_errors = 0;
	}
	else {
		scram_data((uint8_t*)f.data, this->data);
	}

	return f;
}

rf_frame frame::encode() const
{
	rf_frame f;

	uint_fast32_t encoded_ambe_1, encoded_ambe_2;
	uint8_t deinterleaved[9];

	std::memcpy(&deinterleaved[6], &ambe[0], 3);

	encoded_ambe_2 = gorlay_encode24128(ambe_decode_2) ^ PRNG_TABLE[ambe_decode_1];
	encoded_ambe_1 = gorlay_encode24128(ambe_decode_1);
	deinterleaved[0] = (encoded_ambe_1 >> 16) & 0xFFU;
	deinterleaved[1] = (encoded_ambe_1 >> 8) & 0xFFU;
	deinterleaved[2] = (encoded_ambe_1) & 0xFFU;
	deinterleaved[3] = (encoded_ambe_2 >> 16) & 0xFFU;
	deinterleaved[4] = (encoded_ambe_2 >> 8) & 0xFFU;
	deinterleaved[5] = (encoded_ambe_2) & 0xFFU;

	ambefec_interleave(f.ambe, deinterleaved);

	if (is_sync() || is_preend()) {
		// We don't decode the sync frame, because it is excluded from
		// scrambling.
		std::memcpy(f.data, data, sizeof(f.data));
	} else if (is_end()) {
		std::memcpy(f.data, data, sizeof(f.data));
		std::memcpy(f.ambe, rf_ambe_end, sizeof(f.ambe));
	}
	else {
		scram_data(f.data, (uint8_t*)this->data);
	}

	return f;
}

}// namespace dv
