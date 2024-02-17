#pragma once
#include <string>
#include <vector>
#include <fstream>
using std::string;
using std::vector;

class Key_Value_File
{
public:
	bool open(const char* filename);
	struct Entry {
		string name;
		vector<vector<string>> tokenized_lines;
		int linenum;
	};
	vector<Entry> entries;
};

extern void tokenize_string(string& input, std::vector<string>& out);

bool Key_Value_File::open(const char* filename)
{
	std::ifstream infile(filename);
	if (!infile)
		return false;

	std::string line;
	Entry cur;
	int linenum = 0;
	while (std::getline(infile, line))
	{
		linenum++;

		// FIXME: white space on lines
		if (line.empty()) continue;
		if (line.at(0) == '#') continue;
		if (line.at(0) != '\t') {
			if (!cur.name.empty()) {
				entries.push_back(cur);
			}
			cur.tokenized_lines.clear();
			cur.name = std::move(line);
			cur.linenum = linenum;
		}
		else {
			int s = cur.tokenized_lines.size();
			cur.tokenized_lines.resize(s + 1);
			tokenize_string(line, cur.tokenized_lines.at(s));
			if (cur.tokenized_lines.at(s).empty())
				cur.tokenized_lines.resize(s);
		}
	}

	if (!cur.name.empty())
		entries.push_back(cur);

	return true;
}