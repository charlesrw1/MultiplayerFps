#pragma once
#include <string>
#include "Framework/StringUtil.h"
inline std::string get_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return name.substr(find);
}
inline std::string get_extension_no_dot(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	if (find >= name.size() - 1)
		return {};
	return name.substr(find+1);
}


inline std::string strip_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return name.substr(0, find);
}
inline bool has_extension(const std::string& path, const std::string& ext)
{
	auto find = path.rfind('.');
	if (find == std::string::npos)
		return false;
	return path.substr(find + 1) == ext;
}


inline void remove_extension(std::string& file)
{
	auto find = file.rfind('.');
	if (find != std::string::npos)
		file.resize(find);
}

inline void get_filename(std::string& file)
{
	auto find = file.rfind('/');
	if (find != std::string::npos) {
		file = file.substr(find + 1);
		remove_extension(file);
	}
}

inline std::string to_std_string_sv(StringView sv)
{
	return std::string(sv.str_start, sv.str_len);
}


inline std::string to_lower(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (auto c : s)
		out.push_back(tolower(c));
	return out;
}