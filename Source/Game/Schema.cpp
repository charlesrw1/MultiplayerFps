#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "Assets/AssetRegistry.h"
#include "Level.h"


CLASS_IMPL(Schema);


#include "EditorDocPublic.h"
class SchemaAssetMetadata : public AssetMetadata
{
public:
	virtual std::string get_type_name() const { return "Schema"; }
	// color of type in browser
	virtual Color32 get_browser_color() const { return { 173, 88, 23 }; }

	virtual void index_assets(std::vector<std::string>& filepaths) const {}
	// return the base filepath for indexed assets, like ./Data/Models
	virtual bool assets_are_filepaths() const { return true; }
	virtual IEditorTool* tool_to_edit_me() const { 
		return g_editor_doc; 
	}
	const char* get_arg_for_editortool() const override {
		return "schema";
	}
	// return <AssetName>::StaticType
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Schema::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(SchemaAssetMetadata);
#include "Assets/AssetDatabase.h"

bool Schema::load_asset(ClassBase*&)
{
	std::string path = get_name();
	auto file = FileSys::open_read_game(path.c_str());
	if (!file)
		return false;

	properties = std::string(file->size(), 0);
	file->read((char*)properties.data(), properties.size());

	default_schema_obj = create_entity_from_properties_internal();

	return default_schema_obj != nullptr;
}

void Schema::sweep_references() const
{
	// create and uncreate to mark refs
	auto temp = create_entity_from_properties_internal();
	delete temp;
}


Entity* Schema::create_entity_from_properties_internal() const
{
	auto ents = LevelSerialization::unserialize_entities_from_string(properties);
	if (ents.size() != 1) {
		sys_print("!!! bad Schema file for %s\n", get_name().c_str());
		for (auto e : ents) delete e;
		return nullptr;
	}
	return ents[0];
}