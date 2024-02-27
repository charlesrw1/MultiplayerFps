#pragma once
#include <cstdint>
#include <cstring>

template<int BUFSIZE>
class Stack_String
{
public:
	Stack_String() {
		data_len = 0;
		data[0] = 0;
	}
	Stack_String(const char* str) {
		int s = strlen(str);
		if (s + 1 < BUFSIZE) {
			memcpy(data, str, s);
			data[s] = 0;
			data_len = s;
		}
	}
	Stack_String(const char* str, int len) {
		int s = strlen(str);
		if (len < s) s = len;
		if (s + 1 < BUFSIZE) {
			memcpy(data, str, s);
			data[s] = 0;
			data_len = s;
		}
	}
	int size() { return data_len; }
	const char* c_str() { return data; }
private:
	int data_len = 0;
	char data[BUFSIZE];
};