#pragma once
#include <memory>
#include <cstdint>
#include "Assets/IAsset.h"
#include "Framework/Reflection2.h"

class LuaClassTypeInfo;

// Base class for lightweight, reflected, JSON-backed data assets — mirrors Unity's
// ScriptableObject / Unreal's UDeveloperSettings. Subclass it, add REF fields (C++ or Lua-
// defined), and load/save/editor-inspector support come for free via reflection:
//
//   class MyWeaponConfig : public ScriptableObject {
//       CLASS_BODY(MyWeaponConfig);
//       REF float damage = 10.f;
//       REF float fire_rate = 0.2f;
//   };
//
// Reference other ScriptableObjects the normal way: REF AssetPtr<MyWeaponConfig> config;
//
// Every concrete subclass shares the single ".sobj" file extension. Because the concrete
// C++/Lua type can't be known until the file is read, the file stores a "__classname" key and
// AssetDatabaseImpl::load_asset_sync special-cases ScriptableObject: it peeks the classname
// and resolves+allocates the concrete type BEFORE calling load_asset(), instead of the usual
// alloc-by-caller's-static-type. See ScriptableObject::peek_concrete_type.
class ScriptableObject : public IAsset
{
public:
	CLASS_BODY(ScriptableObject, scriptable);

	bool load_asset() final;
	void post_load() final {}
	void uninstall() final;

	// Writes this object's reflected fields back to its file. Editor-only convention method
	// (not part of the IAsset interface), same pattern as PostProcessSettings::save_to_disk.
	void save_to_disk();

	// Reads just the "__classname" key out of a .sobj file without allocating anything.
	// Returns nullptr if the file can't be read/parsed or the classname isn't registered.
	static const ClassTypeInfo* peek_concrete_type(const std::string& path);

	// Synchronous, Lua-callable generic loader: given a concrete ScriptableObject subclass
	// type and a game-relative .sobj path, returns the (possibly already-loaded) asset
	// instance. lua_generic makes the generated Lua stub report the return type as whatever
	// `type` is, e.g. `MyWeaponConfig.load(MyWeaponConfig, "path")` types as `MyWeaponConfig`.
	REFLECT(lua_generic);
	static ScriptableObject* load(const ClassTypeInfo* type, const std::string& path);

	// Called every frame this asset is open in the Asset Inspector, after the reflected
	// property grid is drawn. Override to draw extra custom controls (Lua or C++).
	REF virtual void on_editor_gui() {}
	// Called once whenever the property grid records an edit to a reflected field.
	REF virtual void on_property_change() {}

	// Per-instance byte buffer backing PROP_LUA_BACKED fields for Lua-defined subclasses.
	// Duplicated from Component's identical mechanism (EntityComponent.h) rather than shared
	// through a new ClassBase virtual — see docs discussion on keeping ClassBase itself free
	// of asset-family-specific state.
	uint8_t* get_lua_field_shadow() const override { return lua_field_shadow.get(); }
	void ensure_lua_shadow() override;
	void take_lua_field_shadow(std::unique_ptr<uint8_t[]> buf) { lua_field_shadow = std::move(buf); }
	void set_lua_owner_type(const LuaClassTypeInfo* t) { lua_owner_type = t; }
	const LuaClassTypeInfo* get_lua_owner_type() const override { return lua_owner_type; }

private:
	std::unique_ptr<uint8_t[]> lua_field_shadow;
	const LuaClassTypeInfo* lua_owner_type = nullptr;
};
