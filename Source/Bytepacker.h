#ifndef SERIALIZATION_H
#define SERIALIZATION_H
#include <cstdint>
#include <cstring>
#include "glm/glm.hpp"
#include "Util.h"

class ByteWriter
{
public:
	ByteWriter(uint8_t* data, int data_len) 
	{
		ASSERT(data_len % 4 == 0 &&"Bitwriter buffer must be multiple of 4 bytes!\n");
		buffer_size = data_len / 4;
		ASSERT(buffer_size > 0);
		buffer = (uint32_t*)data;
	}
	//int BytesWritten() const { return data_ptr; }
	bool HasFailed() const { return failed; }
	int BytesWritten() {
		if (scratch_bits != 0)
			printf("byteswritten called when data left in scratch (call EndWrite())\n");
		return word_index*4;
	}

	void WriteBits(uint32_t val, int numbits = 32) {
		assert(numbits >= 1 && numbits <= 32);

		val = (uint64_t)val & ((1ll << numbits) - 1ll);

		if (CheckOverrun(numbits))
			return;

		scratch |= (uint64_t)val << scratch_bits;
		scratch_bits += numbits;
		
		//bitswritten += numbits;

		if (scratch_bits >= 32) {
			FlushScratch();
		}

		assert(scratch < ((1ll << scratch_bits)));
		assert(scratch_bits == 0 && scratch == 0 || scratch_bits > 0);
	}
	void WriteBool(bool b) {
		WriteBits(b, 1);
	}
	void WriteByte(uint8_t byte) {
		WriteBits(byte, 8);
	}
	void WriteShort(uint16_t word) {
		WriteBits(word, 16);
	}
	void WriteLong(uint32_t dword) {
		WriteBits(dword, 32);
	}
	void WriteFloat(float f) {
		union {
			uint32_t ival;
			float fval;
		}x;
		x.fval = f;
		WriteLong(x.ival);
	}
	void WriteVec3(glm::vec3 v) {
		WriteFloat(v.x);
		WriteFloat(v.y);
		WriteFloat(v.z);
	}

	void WriteBytes(const uint8_t* src, int num_bytes) {
		for (int i = 0; i < num_bytes; i++) {
			WriteByte(src[i]);
		}
	}

	void EndWrite() {
		int bits_to_align_to_word = (32 - (scratch_bits % 32)) % 32;
		if(bits_to_align_to_word!=0)
			WriteBits(0, bits_to_align_to_word);
		ASSERT(scratch_bits == 0);
	}

	void FlushScratch() {
		ASSERT(word_index < buffer_size);
		if (word_index >= buffer_size) {
			Fatalf("bitpacker overrun\n");
		}
		ASSERT(scratch_bits >= 32);
		uint32_t lower_word = scratch & (uint64_t)UINT32_MAX;
		scratch >>= 32;
		scratch_bits -= 32;
		buffer[word_index++] = lower_word;
	}

	void AlignToByteBoundary() {
		int bits_to_align = (8 - (scratch_bits % 8)) % 8;
		WriteBits(0, bits_to_align);
		ASSERT(((8 - (scratch_bits % 8)) % 8) == 0);
	}
private:
	bool CheckOverrun(int bits_to_write) {
		int bits_in_scratch = scratch_bits + bits_to_write;
		int words_pending = (bits_in_scratch / 32) + (bits_in_scratch % 32 != 0);

		if (words_pending + word_index > buffer_size) {
			failed = true;
		}
		return failed;
	}

	bool failed = false;

	uint64_t scratch = 0;
	int scratch_bits = 0;
	int word_index = 0;
	uint32_t* buffer = nullptr;
	int buffer_size = 0;	// in multiple of 4 bytes !!!
};

class ByteReader
{
public:
	ByteReader(const uint8_t* data, int data_len) {
		if (data_len % 4 != 0) {
			printf("bitreader buffer must be multiple of 4 bytes\n");
			failed = true;
		}
		total_bits = data_len * 8;
		buffer_len = data_len / 4;
		ASSERT(buffer_len != 0);
		buffer = (uint32_t*)data;
	}
	bool HasFailed() const { return failed; }
	bool IsEof() const {
		return !failed && num_bits_read == total_bits;
	}

	uint32_t ReadBits(int numbits) {
		if (CheckOverrun(numbits))
			return 0;
		if (scratch_bits - numbits < 0)
			ReadNextWord();

		uint32_t lowbits = scratch & ((1ll << numbits) - 1ll);
		scratch >>= numbits;
		scratch_bits -= numbits;

		num_bits_read += numbits;

		return lowbits;
	}
	bool ReadBool() {
		return ReadBits(1);
	}
	uint8_t ReadByte() {
		return ReadBits(8);
	}
	uint16_t ReadShort() {
		return ReadBits(16);
	}
	uint32_t ReadLong() {
		return ReadBits(32);
	}
	float ReadFloat() {
		union {
			uint32_t ival;
			float fval;
		}x;
		x.ival = ReadBits(32);
		return x.fval;
	}
	glm::vec3 ReadVec3() {
		float x = ReadFloat();
		float y = ReadFloat();
		float z = ReadFloat();
		return glm::vec3(x, y, z);
	}
	void ReadBytes(uint8_t* dest, int num_bytes) {
		for (int i = 0; i < num_bytes; i++) {
			dest[i] = ReadByte();
		}
	}
	void AlignToByteBoundary() {
		int remainder_bits = scratch_bits % 8;
		if (remainder_bits != 0) {
			int parity_bits = ReadBits(remainder_bits);
			ASSERT(scratch_bits % 8 == 0);
			if (parity_bits != 0) {
				failed = true;
				printf("bitreader read align failed\n");
			}
		}
	}

private:
	void ReadNextWord() {
		if (word_index >= buffer_len) {
			failed = true;
			printf("buffer overrun bitreader\n");
			return;
		}

		ASSERT(word_index < buffer_len);
		ASSERT(scratch_bits < 32);
		uint32_t nextword = buffer[word_index++];
		scratch |= (uint64_t)nextword << scratch_bits;
		scratch_bits += 32;
	}
	bool CheckOverrun(int bits_to_read) {
		if (num_bits_read + bits_to_read > total_bits) {
			failed = true;
			return true;
		}
		return false;
	}
	bool failed = false;

	uint64_t scratch = 0;
	const uint32_t* buffer = nullptr;
	int buffer_len = 0;
	int word_index = 0;
	int scratch_bits = 0;
	int num_bits_read = 0;
	int total_bits = 0;
};

#endif // !SERIALIZATION_H