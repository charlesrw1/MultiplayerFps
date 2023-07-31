#ifndef SERIALIZATION_H
#define SERIALIZATION_H
#include <cstdint>
#include <cstring>
class ByteWriter
{
public:
	ByteWriter(uint8_t* data, int data_len) : data(data), data_len(data_len) {}
	int BytesWritten() const { return data_ptr; }
	bool HasFailed() const { return failed; }
	void WriteByte(uint8_t byte) {
		if (!CheckOverrun(1)) {
			data[data_ptr++] = byte;
		}
	}
	void WriteWord(uint16_t word) {
		if (!CheckOverrun(2)) {
			data[data_ptr] = word & 0xff;
			data[data_ptr + 1] = (word >> 8) & 0xff;
			data_ptr += 2;
		}
	}
	void WriteLong(uint32_t dword) {
		if (!CheckOverrun(4)) {
			data[data_ptr] = dword & 0xff;
			data[data_ptr + 1] = (dword >> 8) & 0xff;
			data[data_ptr + 2] = (dword >> 16) & 0xff;
			data[data_ptr + 3] = (dword >> 24) & 0xff;
			data_ptr += 4;
		}
	}
	void WriteFloat(float f) {
		union {
			uint32_t ival;
			float fval;
		}x;
		x.fval = f;
		WriteLong(x.ival);
	}
	void WriteBytes(const uint8_t* src, int num_bytes) {
		if (!CheckOverrun(num_bytes)) {
			memcpy(&data[data_ptr], src, num_bytes);
			data_ptr += num_bytes;
		}
	}

private:
	bool CheckOverrun(int bytes_to_read) {
		if (data_ptr + bytes_to_read > data_len) {
			failed = true;
			return true;
		}
		return false;
	}
	bool failed = false;
	int data_ptr = 0;
	uint8_t* const data = nullptr;
	const int data_len = 0;
};

class ByteReader
{
public:
	ByteReader(const uint8_t* data, int data_len) : data(data), data_len(data_len) {}
	bool HasFailed() const { return failed; }
	bool IsEof() const {
		return !failed && data_len == data_ptr;
	}
	uint8_t ReadByte() {
		if (CheckOverrun(1))
			return 0;
		return data[data_ptr++];
	}
	uint16_t ReadWord() {
		if (CheckOverrun(2))
			return 0;
		uint16_t o = data[data_ptr] | data[data_ptr + 1] << 8;
		data_ptr += 2;
		return o;
	}
	uint32_t ReadLong() {
		if (CheckOverrun(4))
			return 0;
		uint32_t o = data[data_ptr] | data[data_ptr + 1] << 8
			| data[data_ptr + 2] << 16 | data[data_ptr + 3] << 24;
		data_ptr += 4;
		return o;
	}
	float ReadFloat() {
		union {
			uint32_t ival;
			float fval;
		}x;
		x.ival = ReadLong();
		return x.fval;
	}
	void ReadBytes(uint8_t* dest, int num_bytes) {
		if (CheckOverrun(num_bytes))
			return;
		memcpy(dest, &data[data_ptr], num_bytes);
		data_ptr += num_bytes;
	}

private:
	bool CheckOverrun(int bytes_to_read) {
		if (data_ptr + bytes_to_read > data_len) {
			failed = true;
			return true;
		}
		return false;
	}
	bool failed = false;
	int data_ptr = 0;
	const uint8_t* const data = nullptr;
	const int data_len = 0;
};

#endif // !SERIALIZATION_H