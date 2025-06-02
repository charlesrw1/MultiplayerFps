#pragma once
#include "Framework/MulticastDelegate.h"
#include <vector>
#include <string>

using std::vector;
using std::string;
class Entity;

// a | b c | d = a or (b and c) or d
class OONameFilter
{
public:
	static bool is_in_string(const string& filter, const string& match);
	static bool does_entity_pass_one_filter(const string& s, Entity* e);
	static vector<vector<string>> parse_into_and_ors(const std::string& filter);
	static bool does_entity_pass(const vector<vector<string>>& filter, Entity* e);
	void draw();
	std::string get_filter() {
		return filter_component;
	}
	MulticastDelegate<std::string> on_filter_enter;
private:
	std::string filter_component;
};