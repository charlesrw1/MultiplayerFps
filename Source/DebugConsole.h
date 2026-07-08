#pragma once
#include <vector>
#include <string>
#include "Framework/Util.h"
using std::string;
using std::vector;
class Debug_Console
{
public:
	static Debug_Console* inst;

	static const int INPUT_BUFFER_SIZE = 256;
	Debug_Console() { memset(input_buffer, 0, sizeof input_buffer); }
	void init();
	void draw();
	void print(LogType type, const char* fmt, ...);
	void print_args(LogType type, const char* fmt, va_list list);
	void toggle_set_focus() { wants_toggle_set_focus = true; }
	bool get_is_console_focused() const { return is_console_focused; }
	struct LineAndColor
	{
		string line;
		Color32 color;
		LogType type;
	};
	vector<LineAndColor> bufferedLines;
	vector<LineAndColor> lines;
	vector<string> history;
	int history_index = -1;
	bool auto_scroll = true;
	bool scroll_to_bottom = false;
	bool wants_toggle_set_focus = false;
	char input_buffer[INPUT_BUFFER_SIZE];

	bool is_console_focused = false;

	// Indexed by LogType. Controls which levels are drawn in the log view.
	bool show_level[5] = { true, true, true, true, true };

	// Line-range selection for click/shift-click copy support. -1 == no selection.
	int select_anchor_line = -1;
	int select_end_line = -1;
};