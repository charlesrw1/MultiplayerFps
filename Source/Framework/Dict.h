#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <cstdarg>
#include "glm/glm.hpp"
#include "Framework/Util.h"

class Dict
{
public:
	void set_string(const char* key, const char* val) {
		keyvalues[key] = val;
	}
	void set_vec3(const char* key, glm::vec3 val) {
		keyvalues[key] = string_format("%f %f %f", val.x, val.y, val.z);
	}
	void set_vec4(const char* key, glm::vec4 val) {
		keyvalues[key] = string_format("%f %f %f %f", val.x, val.y, val.z,val.w);
	}
	void set_float(const char* key, float val) {
		keyvalues[key] = string_format("%f", val);
	}
	void set_int(const char* key, int val) {
		keyvalues[key] = string_format("%d", val);
	}

	const char* get_string(const char* key, const char* defaultval = "") const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		return find->second.c_str();
	}

	Color32 get_color(const char* key, Color32 defaultval = COLOR_WHITE) const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		Color32 o;
		int fields = sscanf(find->second.c_str(), "%hhu %hhu %hhu %hhu", &o.r, &o.g, &o.b,&o.a);
		return o;
	}
	void set_color(const char* key, Color32 c) {
		keyvalues[key] = string_format("%hhu %hhu %hhu %hhu", c.r, c.g, c.b, c.a);
	}

	glm::vec3 get_vec3(const char* key, glm::vec3 defaultval = glm::vec3(0.f))const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		glm::vec3 o = glm::vec3(0.f);
		int fields = sscanf(find->second.c_str(), "%f %f %f", &o.x, &o.y, &o.z);
		return o;
	}
	glm::vec4 get_vec4(const char* key, glm::vec4 defaultval = glm::vec4(0.f))const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		glm::vec4 o = glm::vec4(0.f);
		int fields = sscanf(find->second.c_str(), "%f %f %f %f", &o.x, &o.y, &o.z,&o.w);
		return o;
	}
	glm::quat get_quat(const char* key, glm::quat defaultval = glm::quat(1, 0, 0, 0)) const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		glm::quat q;
		int fields = sscanf(find->second.c_str(), "%f %f %f %f", &q.w, &q.x, &q.y, &q.z);
		return q;
	}
	glm::vec2 get_vec2(const char* key, glm::vec2 defaultval = glm::vec2(0.f)) const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		glm::vec2 o = glm::vec2(0.f);
		int fields = sscanf(find->second.c_str(), "%f %f", &o.x, &o.y);
		return o;
	}
	float get_float(const char* key, float defaultval = 0.f)const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		float o = 0.f;
		int fields = sscanf(find->second.c_str(), "%f",&o);
		return o;
	}
	int get_int(const char* key, int defaultval = 0)const {
		const auto& find = keyvalues.find(key);
		if (find == keyvalues.end())
			return defaultval;
		int o = 0;
		int fields = sscanf(find->second.c_str(), "%d", &o);
		return o;
	}
	void clear() {
		keyvalues.clear();
	}
	void remove_key(const char* key) {
		keyvalues.erase(key);
	}
	bool has_key(const char* key) {
		const auto& find = keyvalues.find(key);
		return !(find == keyvalues.end());
	}

	std::unordered_map <std::string, std::string> keyvalues;
};
