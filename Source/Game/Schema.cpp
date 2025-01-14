#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "Assets/AssetRegistry.h"
#include "Level.h"


CLASS_IMPL(Schema);


extern IEditorTool* g_editor_doc;

class SchemaAssetMetadata : public AssetMetadata
{
public:
	virtual std::string get_type_name() const { return "Schema"; }
	// color of type in browser
	virtual Color32 get_browser_color() const { return { 173, 88, 23 }; }

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
	return false;
}

void Schema::sweep_references() const
{

}


Entity* Schema::create_entity_from_properties_internal() const
{
	return nullptr;
}