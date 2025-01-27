#include "DataClass.h"

#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"

#include "AssetCompile/Someutils.h"//fixme, for has_extension

#include "Assets/AssetDatabase.h"

class IEditorTool;
extern IEditorTool* g_dataclass_editor;	// defined in MiscEditors/DataClassEditor.h

CLASS_IMPL(DataClass);

#ifdef EDITOR_BUILD
class DataClassAssetMetadata : public AssetMetadata
{
public:
	DataClassAssetMetadata() {
		extensions.push_back("dc");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 50, 168, 151 };
	}

	virtual std::string get_type_name() const  override
	{
		return "DataClass";
	}

	virtual IEditorTool* tool_to_edit_me() const override { return g_dataclass_editor; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &DataClass::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(DataClassAssetMetadata);
#endif
#include "Framework/ObjectSerialization.h"


bool DataClass::load_asset(ClassBase*&)
{
	assert(object == nullptr);


	auto file = FileSys::open_read_game(get_name());
	if (!file.get()) {
		sys_print(Error, "couldnt load dataclass (file not found): %s\n", get_name().c_str());
		return false;
	}

	DictParser dp;
	dp.load_from_file(file.get());
	StringView tok;
	dp.read_string(tok);
	auto classLoaded = read_object_properties<ClassBase>(nullptr, dp, tok);
	if (!classLoaded) {
		sys_print(Error, "couldnt load dataclass (parse error): %s\n", get_name().c_str());
		return false;
	}

	object = classLoaded;
}
void DataClass::sweep_references() const
{
	// im lazy af rn
	// this marks references though
	
	std::string fullpath = get_name();
	auto file = FileSys::open_read_game(fullpath.c_str());
	if (!file.get()) {
		sys_print(Error, "couldnt load dataclass (file not found): %s\n", fullpath.c_str());
		return;
	}

	DictParser dp;
	dp.load_from_file(file.get());
	StringView tok;
	dp.read_string(tok);
	auto classLoaded = read_object_properties<ClassBase>(nullptr, dp, tok);
	if (!classLoaded) {
		sys_print(Error, "couldnt load dataclass (parse error): %s\n", fullpath.c_str());
		return;
	}

	delete classLoaded;
}
