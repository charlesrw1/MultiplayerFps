#pragma once
// fixme

class GameTagManager
{
public:
	static GameTagManager& get() {
		static GameTagManager inst;
		return inst;
	}
	void add_tag(const std::string& tag) {
		registered_tags.insert(tag);
	}
	std::unordered_set<std::string> registered_tags;
};