#pragma once
#include <string>
#include <json.hpp>

// The saved fields of one object, before they are read: what ClassBase::upgrade_data changes.
class SavedFields
{
public:
	explicit SavedFields(nlohmann::json& data) : data(data) { ASSERT(data.is_object()); }

	bool has(const std::string& name) const { return data.contains(name); }
	// Moves the value of from to to; does nothing when from is missing or to exists.
	void rename(const std::string& from, const std::string& to) {
		if (!has(from) || has(to))
			return;
		data[to] = std::move(data[from]);
		data.erase(from);
	}
	void remove(const std::string& name) { data.erase(name); }
	nlohmann::json& json() { return data; }

private:
	nlohmann::json& data;
};
