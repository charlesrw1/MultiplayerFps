#include "DataClass.h"

#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"

#include "AssetCompile/Someutils.h"//fixme, for has_extension

#include "Assets/AssetDatabase.h"

class IEditorTool;
extern IEditorTool* g_dataclass_editor;	// defined in MiscEditors/DataClassEditor.h

CLASS_IMPL(DataClass);

static const char* const DATACLASS_FOLDER = "./Data/";

class DataClassAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 50, 168, 151 };
	}

	virtual std::string get_type_name() const  override
	{
		return "DataClass";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override
	{
		auto find_tree = FileSys::find_files(DATACLASS_FOLDER);
		for (const auto _file : find_tree) {
			auto file = _file.substr(8);
			if (has_extension(file, "dc")) {
				std::string path = strip_extension(file);
				filepaths.push_back(file);
			}
		}
	}
	virtual IEditorTool* tool_to_edit_me() const override { return g_dataclass_editor; }
	virtual std::string root_filepath() const  override
	{
		return DATACLASS_FOLDER;
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &DataClass::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(DataClassAssetMetadata);

#include "Framework/ObjectSerialization.h"


bool DataClass::load_asset(ClassBase*&)
{
	assert(object == nullptr);

	std::string fullpath = DATACLASS_FOLDER + get_name();
	auto file = FileSys::open_read(fullpath.c_str());
	if (!file.get()) {
		sys_print("!!! couldnt load dataclass (file not found): %s\n", fullpath.c_str());
		return false;
	}

	DictParser dp;
	dp.load_from_file(file.get());
	StringView tok;
	dp.read_string(tok);
	auto classLoaded = read_object_properties<ClassBase>(nullptr, dp, tok);
	if (!classLoaded) {
		sys_print("!!! couldnt load dataclass (parse error): %s\n", fullpath.c_str());
		return false;
	}

	object = classLoaded;
}
void DataClass::sweep_references() const
{
	// im lazy af rn
	// this marks references though
	
	std::string fullpath = DATACLASS_FOLDER + get_name();
	auto file = FileSys::open_read(fullpath.c_str());
	if (!file.get()) {
		sys_print("!!! couldnt load dataclass (file not found): %s\n", fullpath.c_str());
		return;
	}

	DictParser dp;
	dp.load_from_file(file.get());
	StringView tok;
	dp.read_string(tok);
	auto classLoaded = read_object_properties<ClassBase>(nullptr, dp, tok);
	if (!classLoaded) {
		sys_print("!!! couldnt load dataclass (parse error): %s\n", fullpath.c_str());
		return;
	}

	delete classLoaded;
}
