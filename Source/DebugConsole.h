#pragma once
#include <vector>
#include <string>
using std::string;
using std::vector;
class Debug_Console
{
public:
	static Debug_Console* inst;

	static const int INPUT_BUFFER_SIZE = 256;
	Debug_Console() {
		memset(input_buffer, 0, sizeof input_buffer);
	}
	void init();
	void draw();
	void print(const char* fmt, ...);
	void print_args(const char* fmt, va_list list);

	vector<string> bufferedLines;
	vector<string> lines;
	vector<string> history;
	int history_index = -1;
	bool auto_scroll = true;
	bool scroll_to_bottom = false;
	bool set_keyboard_focus = false;
	char input_buffer[INPUT_BUFFER_SIZE];
};