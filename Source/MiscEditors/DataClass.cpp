#include "DataClass.h"

#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"

#include "AssetCompile/Someutils.h"//fixme, for has_extension

class IEditorTool;
extern IEditorTool* g_dataclass_editor;	// defined in MiscEditors/DataClassEditor.h


DataClassLoader g_dc_loader;

CLASS_IMPL(DataClass);
REGISTERASSETLOADER_MACRO(DataClass, &g_dc_loader);

static const char* const DATACLASS_FOLDER = "./Data/DataClasses/";

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
			auto file = _file.substr(20);
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

const DataClass* DataClassLoader::load_dataclass_no_check(const std::string& filepath)
{
	if (all_dataclasses.find(filepath) != all_dataclasses.end())
		return all_dataclasses.find(filepath)->second;

	std::string fullpath = DATACLASS_FOLDER + filepath;
	auto file = FileSys::open_read(fullpath.c_str());
	if (!file.get()) {
		sys_print("!!! couldnt load dataclass (file not found): %s\n", filepath.c_str());
		return nullptr;
	}

	DictParser dp;
	dp.load_from_file(file.get());
	StringView tok;
	dp.read_string(tok);
	auto classLoaded = read_object_properties<ClassBase>(nullptr, dp, tok);
	if (!classLoaded) {
		sys_print("!!! couldnt load dataclass (parse error): %s\n", filepath.c_str());
		return nullptr;
	}

	DataClass* dc = new DataClass;
	dc->object = classLoaded;
	dc->path = filepath;
	all_dataclasses.insert({ filepath,dc });

	return dc;
}