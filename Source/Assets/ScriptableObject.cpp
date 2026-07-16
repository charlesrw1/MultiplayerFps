#include "ScriptableObject.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include "Framework/SerializerJson2.h"
#include "Scripting/ScriptManager.h"
#include "Assets/AssetDatabase.h"
#include <json.hpp>

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"

class ScriptableObjectMetadata : public AssetMetadata {
public:
	ScriptableObjectMetadata() { extensions.push_back("sobj"); }
	Color32 get_browser_color() const override { return {200, 160, 80}; }
	std::string get_type_name() const override { return "ScriptableObject"; }
	const ClassTypeInfo* get_asset_class_type() const override { return &ScriptableObject::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(ScriptableObjectMetadata);
#endif

static bool read_game_file_text(const std::string& path, std::string& out) {
	auto file = FileSys::open_read_game(path);
	if (!file)
		return false;
	size_t sz = file->size();
	out.resize(sz, '\0');
	file->read(out.data(), sz);
	file->close();
	return true;
}

const ClassTypeInfo* ScriptableObject::peek_concrete_type(const std::string& path) {
	std::string text;
	if (!read_game_file_text(path, text)) {
		sys_print(Error, "ScriptableObject::peek_concrete_type: couldn't read file\n");
		return nullptr;
	}
	try {
		auto j = nlohmann::json::parse(text);
		std::string classname = j.value("__classname", std::string());
		if (classname.empty())
			return nullptr;
		return ClassBase::find_class(classname.c_str());
	}
	catch (const nlohmann::json::exception&) {
		return nullptr;
	}
}

ScriptableObject* ScriptableObject::load(const ClassTypeInfo* type, const std::string& path) {
	if (!type || !type->is_a(ScriptableObject::StaticType))
		return nullptr;
	auto sptr = g_assets.find_sync_sptr(path, type);
	return sptr ? static_cast<ScriptableObject*>(sptr.get()) : nullptr;
}

bool ScriptableObject::load_asset() {
	load_warnings.clear();

	std::string text;
	if (!read_game_file_text(get_name(), text)) {
		sys_print(Warning, "ScriptableObject: failed to open %s\n", get_name().c_str());
		return false;
	}

	nlohmann::json j;
	try {
		j = nlohmann::json::parse(text);
	}
	catch (const nlohmann::json::exception& e) {
		sys_print(Warning, "ScriptableObject: JSON error in %s: %s\n", get_name().c_str(), e.what());
		return false;
	}

	// __classname was already consumed by peek_concrete_type to pick this instance's
	// concrete type. A value whose JSON type doesn't match its reflected field (e.g. a string
	// where a float is expected) throws mid-read; caught here so one bad field degrades to a
	// warning instead of taking the whole asset load down with it -- fields read before the
	// bad one keep their values, fields after it keep their in-memory defaults.
	try {
		ReadSerializerBackendJson2 reader("sobj", j, *this);
		for (auto it = j.begin(); it != j.end(); ++it) {
			const std::string& key = it.key();
			if (key.size() >= 2 && key[0] == '_' && key[1] == '_')
				continue; // __classname etc. -- meta keys handled outside reflection
			if (reader.get_consumed_keys().count(key) == 0) {
				std::string msg = "unknown property '" + key + "' (typo or stale field?)";
				sys_print(Warning, "ScriptableObject: %s: %s\n", get_name().c_str(), msg.c_str());
				load_warnings.push_back(std::move(msg));
			}
		}
	}
	catch (const nlohmann::json::exception& e) {
		std::string msg = std::string("bad property value: ") + e.what();
		sys_print(Warning, "ScriptableObject: %s: %s\n", get_name().c_str(), msg.c_str());
		load_warnings.push_back(std::move(msg));
	}
	return true;
}

void ScriptableObject::uninstall() {
	// Do not clear lua_owner_type here: hot-reload re-deserializes this same instance
	// in place (no fresh lua_class_alloc call), so ensure_shadow_for still needs it to
	// re-allocate the shadow buffer on the following load_asset().
	ScriptManager::destroy_shadow_for(this);
}

void ScriptableObject::save_to_disk() {
	WriteSerializerBackendJson2 writer("sobj", *this);
	nlohmann::json j = writer.get_output();
	j["__classname"] = get_type().classname;

	std::string text = j.dump(2);
	auto file = FileSys::open_write_game(get_name());
	if (!file) {
		sys_print(Warning, "ScriptableObject: failed to write %s\n", get_name().c_str());
		return;
	}
	file->write(text.data(), text.size());
	file->close();
}

void ScriptableObject::ensure_lua_shadow() {
	ScriptManager::ensure_shadow_for(this);
}
