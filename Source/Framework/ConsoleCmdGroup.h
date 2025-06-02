#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
template<typename T>
using uptr = std::unique_ptr<T>;
class Cmd_Manager_Impl;
class Cmd_Args;
class ConsoleCmdGroup
{
public:
	static uptr<ConsoleCmdGroup> create(std::string name);
	~ConsoleCmdGroup();
	ConsoleCmdGroup& add(std::string name, const std::function<void(const Cmd_Args&)>&);
	void enable() { enabled = true; }
	void disable() { enabled = false; }
private:
	ConsoleCmdGroup() {}
	bool enabled = true;
	std::string groupname;
	std::unordered_map<std::string, std::function<void(const Cmd_Args&)>> cmds;
	friend class Cmd_Manager_Impl;
};