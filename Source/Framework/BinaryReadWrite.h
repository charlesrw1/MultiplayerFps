#pragma once
#include <cstdint>
#include <cstring>
#include <string>

#include "Framework/Files.h"
#include "Framework/StringUtil.h"

#include <cassert>

class BinaryReader
{
public:
	BinaryReader(IFile* file) {
		size_t len = file->size();
		this->size = len;
		this->data = new uint8_t[len];
		owns_ptr = true;
		file->read(this->data, len);
	}
	~BinaryReader() {
		if (owns_ptr)
			delete[] data;
	}
	BinaryReader(size_t size, uint8_t* data) : data(data),size(size),owns_ptr(false) {}
	BinaryReader(const BinaryReader& other) = delete;
	BinaryReader(BinaryReader&& other) = delete;

	void read_string(std::string& s) {
		uint16_t count = read_int16();
		if (count <= 4000) {
			s.resize(count, ' ');
			read_bytes_ptr((uint8_t*)s.data(), count);
		}
	}
	StringView read_string_view() {
		uint16_t count = read_int16();
		if (!can_read_these_bytes(count))
			return{};// null
		StringView ret = StringView((const char*)&data[ptr], count);
		ptr += count;
		return ret;
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
	bool read_bytes_ptr(void* dest, size_t write_size) {
		if (!can_read_these_bytes(write_size))
			return false;
		std::memcpy(dest, &data[ptr], write_size);
		ptr += write_size;
		return true;
	}
	template<typename T>
	bool read_struct(T* dest) {
		return read_bytes_ptr(dest, sizeof(T));
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


	bool getline(StringView& tok, char delimiter = '\n');
private:
	bool can_read_these_bytes(int count) {
		if (count + ptr > size)
			fail_flag = true;
		return !fail_flag;
	}

	bool owns_ptr = false;
	bool fail_flag = false;
	size_t ptr = 0;
	size_t size = 0;
	uint8_t* data = nullptr;
};

class FileWriter
{
public:
	FileWriter(size_t reserve = 0) {
		buffer.reserve(reserve);
	}
	FileWriter(const FileWriter& other) = delete;
	FileWriter(FileWriter&& other) = delete;

	bool write_out(const std::string& path);

	void write_string(const std::string& str) {
		int len = str.size();
		if (str.size() > 4000) {
			printf("!!! trying to write out string with more than 4000 characters, truncating...\n");
			len = 4000;
		}
		write_int16(len);
		write_bytes_ptr((uint8_t*)str.data(), len);
	}

	void write_byte(uint8_t i) {
		if (ptr == buffer.size())
			buffer.push_back(i);
		else {
			assert(0);
			buffer[ptr] = i;
		}
		ptr++;
	}
	void write_int16(uint16_t i) {
		write_byte(i & 0xff);
		write_byte((i >> 8));
	}
	void write_int32(uint32_t i) {
		write_int16(i & 0xffff);
		write_int16(i >> 16);
	}
	void write_int64(uint64_t i) {
		write_int32(i & 0xffffffff);
		write_int32(i >> 32);
	}
	void write_float(float f) {
		union {
			float f_;
			uint32_t i;
		};
		f_ = f;
		write_int32(i);
	}
	void write_bytes_ptr(const uint8_t* data, size_t size) {
		for (int i = 0; i < size; i++)
			write_byte(data[i]);
	}
	template<typename T>
	void write_struct(const T* t) {
		write_bytes_ptr((uint8_t*)t, sizeof(T));
	}
	void seek(size_t where_) {
		ptr = where_;
		if (ptr > buffer.size())
			buffer.resize(ptr, 0);
	}
	size_t tell() {
		return ptr;
	}


	size_t get_size() const { return buffer.size(); }
	const char* get_buffer() const { return (char*)buffer.data(); }
private:
	size_t ptr = 0;
	std::vector<uint8_t> buffer;
};