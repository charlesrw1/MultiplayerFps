#include "ScriptAsset.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

CLASS_IMPL(Script);


#ifdef EDITOR_BUILD
class ScriptAssetMetaData : public AssetMetadata
{
public:
	ScriptAssetMetaData() {
		extensions.push_back("lua");
		//pre_compilied_extension = "mis";
	}
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 157, 193, 250 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Script";
	}

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Script::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(ScriptAssetMetaData);
#endif

bool Script::load_asset(ClassBase*&)
{
	const auto& path = get_name();
	auto file = FileSys::open_read_game(path);
	if (!file) {
		sys_print(Warning, "couldnt load script\n");
		return false;
	}
	script_str.resize(file->size(), ' ');
	file->read((void*)script_str.data(), file->size());

	return true;
}
void Script::uninstall()
{

}
void Script::move_construct(IAsset* src)
{
	Script* other = (Script*)src;
	script_str = std::move(other->script_str);
}