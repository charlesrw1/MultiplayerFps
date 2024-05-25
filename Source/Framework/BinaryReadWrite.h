#pragma once
#include <cstdint>
#include <cstring>
#include <string>

class FileReader
{
public:
	FileReader(size_t size, uint8_t* data) : data(data),size(size)
	{}
	virtual ~FileReader() = 0;
	FileReader(const FileReader& other) = delete;
	FileReader(FileReader&& other) = delete;


	void read_string(std::string& s) {
		uint16_t count = read_int16();
		if (count <= 4000) {
			s.resize(count, ' ');
			read_bytes_ptr((uint8_t*)s.data(), count);
		}
	}

	uint8_t read_byte() {
		if (!can_read_these_bytes(1))
			return 0;
		return data[ptr++];
	}
	uint16_t read_int16() {
		if (!can_read_these_bytes(2))
			return 0;
		uint16_t out = (uint16_t)data[ptr] | ((uint16_t)data[ptr + 1] << 8);
		ptr+=2;
		return out;
	}
	uint32_t read_int32() {
		if (!can_read_these_bytes(4))
			return 0;
		uint32_t out = (uint32_t)data[ptr] | ((uint32_t)data[ptr + 1] << 8) 
			| ((uint32_t)data[ptr+2] << 16) | ((uint32_t)data[ptr + 3] << 24);
		ptr += 4;
		return out;
	}
	uint64_t read_int64() {
		if (!can_read_these_bytes(8))
			return 0;
		uint32_t out = (uint32_t)data[ptr] | ((uint32_t)data[ptr + 1] << 8)
			| ((uint32_t)data[ptr + 2] << 16) | ((uint32_t)data[ptr + 3] << 24);
		ptr += 4;
		uint32_t outhigh = (uint32_t)data[ptr] | ((uint32_t)data[ptr + 1] << 8)
			| ((uint32_t)data[ptr + 2] << 16) | ((uint32_t)data[ptr + 3] << 24);
		ptr += 4;
		return (uint64_t)out | ((uint64_t)outhigh << 32 );
	}
	float read_float() {
		union {
			uint32_t i;
			float f;
		};
		i = read_int32();
		return f;
	}
	bool read_bytes_ptr(uint8_t* dest, size_t size) {
		if (!can_read_these_bytes(size))
			return false;
		std::memcpy(dest, data, size);
		ptr += size;
		return true;
	}
	bool seek(size_t where_) {
		if (where_ >= size)
			return false;
		ptr = where_;
	}
	size_t tell() {
		return ptr;
	}
	bool has_failed() { return fail_flag; }
	bool is_eof() { return !fail_flag && ptr == size; }
private:
	bool can_read_these_bytes(int count) {
		if (count + ptr > size)
			fail_flag = true;
		return !fail_flag;
	}

	bool fail_flag = false;
	size_t ptr = 0;
	size_t size = 0;
	uint8_t* data = nullptr;
};

class FileReaderOS : public FileReader
{
public:
	~FileReaderOS() override;
};

class FileReaderArchive : public FileReader
{
public:
	~FileReaderArchive() override;
};


class FileWriter
{
public:
	FileWriter(size_t size, uint8_t* data);
	virtual ~FileWriter() = 0;
	FileWriter(const FileWriter& other) = delete;
	FileWriter(FileWriter&& other) = delete;

	void write_byte(uint8_t i);
	void write_int16(uint16_t i);
	void write_int32(uint32_t i);
	void write_int64(uint64_t i);
	void write_float(float f);
	void write_bytes_ptr(const uint8_t* data, size_t size);
	void seek(size_t where_);
	size_t tell();

	size_t ptr = 0;
	size_t size = 0;
	uint8_t* data = nullptr;
};