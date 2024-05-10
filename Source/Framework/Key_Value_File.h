#pragma once
#include <string>
#include <unordered_map>
#include "Framework/Config.h"
#include "Framework/Util.h"
#include "Dict.h"

class Key_Value_File
{
public:
	bool open(const char* filename);
	struct Entry {
		Dict dict;
		int linenum;
	};
	std::unordered_map<std::string, Entry> name_to_entry;
};

extern void tokenize_string(std::string& input, Cmd_Args& out);

inline bool Key_Value_File::open(const char* filename)
{
	File_Buffer* infile = Files::open(filename, Files::TEXT | Files::LOOK_IN_ARCHIVE);
	if (!infile)
		return false;

	name_to_entry.clear();

	char buffer[256];
	buffer[0] = 0;
	Buffer str_buffer{ buffer,256 };
	int getline_index = 0;

	Cmd_Args args;
	std::string line;
	std::string curname;
	std::string temp;
	Entry cur;
	int linenum = 0;
	while (file_getline(infile, &str_buffer, &getline_index,'\n'))
	{
		line = buffer;

		linenum++;

		// FIXME: white space on lines
		if (line.empty()) continue;
		if (line.at(0) == '#') continue;
		if (line.at(0) != '\t') {
			if (!curname.empty()) {
				name_to_entry[curname] = std::move(cur);
			}
			cur.dict.clear();
			curname = std::move(line);
			cur.linenum = linenum;
		}
		else {
			args.clear();
			tokenize_string(line, args);
			// fixme bad
			if (args.size() > 0) {
				temp.clear();
				for (int i = 1; i < args.size(); i++) {
					if (i != 1) temp += ' ';
					temp += args.at(i);
				}

				cur.dict.set_string(args.at(0), temp.c_str());
			}
		}
	}

	if (!curname.empty()) {
		name_to_entry[curname] = std::move(cur);
	}

	return true;
}