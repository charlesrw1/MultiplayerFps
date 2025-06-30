#include "StringUtils.h"

bool StringUtils::is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r';
}

std::string StringUtils::strip(const std::string& str) {
	size_t start = 0, end = str.length();

	while (start < end && is_whitespace(str[start])) start++;  // Skip leading spaces
	while (end > start && is_whitespace(str[end - 1])) end--;  // Skip trailing spaces

	return str.substr(start, end - start);
}

std::vector<std::string> StringUtils::to_lines(const std::string& input, char delim) {
	std::vector<std::string> lines;
	size_t start = 0, end;

	while ((end = input.find(delim, start)) != std::string::npos) {
		lines.push_back(input.substr(start, end - start));
		start = end + 1;
	}

	// Add the last line (if any)
	if (start < input.size()) {
		lines.push_back(input.substr(start));
	}

	return lines;
}
std::vector<std::string> StringUtils::split(const std::string& input)
{
	std::vector<std::string> lines;
	std::string cur;
	for (auto c : input) {
		if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
			if (!cur.empty()) {
				lines.push_back(cur);
				cur.clear();
			}
		}
		else {
			cur += c;
		}
	}
	if (!cur.empty()) {
		lines.push_back(cur);
	}
	return lines;
}
void StringUtils::replace(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

std::string StringUtils::get_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return strip(name.substr(find));
}

std::string StringUtils::get_extension_no_dot(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	if (find >= name.size() - 1)
		return {};
	return strip(name.substr(find + 1));
}

std::string StringUtils::strip_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return strip(name.substr(0, find));
}

bool StringUtils::has_extension(const std::string& path, const std::string& ext)
{
	auto find = path.rfind('.');
	if (find == std::string::npos)
		return false;
	return path.substr(find + 1) == ext;
}

void StringUtils::remove_extension(std::string& file)
{
	auto find = file.rfind('.');
	if (find != std::string::npos)
		file.resize(find);
}

void StringUtils::get_filename(std::string& file)
{
	auto find = file.rfind('/');
	if (find != std::string::npos) {
		file = file.substr(find + 1);
		remove_extension(file);
	}
}

std::string StringUtils::to_lower(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (auto c : s)
		out.push_back(tolower(c));
	return out;
}

bool StringUtils::starts_with(const std::string& str, const std::string& what)
{
	auto find = str.find(what);
	return find == 0;	// found at index 0 
}

string StringUtils::get_directory(const string& input)
{
	auto find = input.rfind('/');
	if (find == std::string::npos) return "";
	if (find == 0) return "";
	return input.substr(0, find);
}

std::string StringUtils::alphanumeric_hash(const std::string& input) {
	// Basic hash computation (FNV-1a hash)
	uint64_t hash = 14695981039346656037ULL;
	for (char c : input) {
		hash ^= static_cast<unsigned char>(c);
		hash *= 1099511628211ULL;
	}
	// Encode using base36 (0-9, a-z) to ensure only alphanumerics
	const char* chars = "0123456789abcdefghijklmnopqrstuvwxyz";
	std::string result;
	while (hash > 0) {
		result.insert(result.begin(), chars[hash % 36]);
		hash /= 36;
	}

	return result;
}
