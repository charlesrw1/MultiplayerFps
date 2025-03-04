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

	return true;
}

static void check_props_for_assetptr(void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto prop = list->list[i];
		if (strcmp(prop.custom_type_str, "AssetPtr") == 0) {
			// wtf!
			IAsset** e = (IAsset**)prop.get_ptr(inst);
			if (*e)
				g_assets.touch_asset(*e);
		}
		else if(prop.type==core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_assetptr(ptr, prop.list_ptr->props_in_list);
			}
		}
	}
}

void DataClass::sweep_references() const
{
	if (!object) {
		sys_print(Warning, "no dataclass object to sweep ref\n");
		return;
	}
	auto type = &object->get_type();
	while (type) {
		auto props = type->props;
		if(props)
			check_props_for_assetptr(object, props);
		type = type->super_typeinfo;
	}
}
