#pragma once
#include <string>
#include <vector>
using std::string;
class StringUtils
{
public:
	static bool is_whitespace(char c);
	static std::string strip(const std::string& str);
	static std::vector<std::string> to_lines(const std::string& input, char delim = '\n');
	static std::vector<std::string> split(const std::string& input);
	static void replace(std::string& str, const std::string& from, const std::string& to);
	static std::string get_extension(const std::string& name);
	static std::string get_extension_no_dot(const std::string& name);
	static std::string strip_extension(const std::string& name);
	static bool has_extension(const std::string& path, const std::string& ext);
	static void remove_extension(std::string& file);
	static void get_filename(std::string& file);
	static std::string to_lower(const std::string& s);
	static std::string to_upper(const std::string& s);
	static bool starts_with(const std::string& str, const std::string& what);
	static string get_directory(const string& path);
	static std::string alphanumeric_hash(const std::string& input);
	static std::string base64_encode(const std::vector<uint8_t>& data);
	static std::vector<uint8_t> base64_decode(const std::string& input);
};
