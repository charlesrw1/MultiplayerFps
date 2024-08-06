#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
#include "Assets/AssetLoaderRegistry.h"
#include "Game/Entity.h"
#include "Assets/AssetRegistry.h"
#include "Level.h"

static const char* schema_base = "./Data/Schema/";

CLASS_IMPL(Schema);
REGISTERASSETLOADER_MACRO(Schema, &g_schema_loader);

#include "EditorDocPublic.h"
class SchemaAssetMetadata : public AssetMetadata
{
public:
	virtual std::string get_type_name() const { return "Schema"; }
	// color of type in browser
	virtual Color32 get_browser_color() const { return { 173, 88, 23 }; }

	virtual void index_assets(std::vector<std::string>& filepaths) const {}
	// return the base filepath for indexed assets, like ./Data/Models
	virtual std::string root_filepath() const { return "./Data/Schema"; }
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

SchemaLoader g_schema_loader;

Schema* SchemaLoader::load_schema(const std::string& schemafile)
{
	if (cached_files.find(schemafile) != cached_files.end())
		return cached_files.find(schemafile)->second;
	std::string path = schema_base + schemafile;
	auto file = FileSys::open_read(path.c_str());
	if (!file)
		return nullptr;
	Schema* s = new Schema;
	s->properties = std::string(file->size(), 0);
	file->read((char*)s->properties.data(), s->properties.size());
	
	bool is_valid = s->init();
	if (is_valid) {
		cached_files[schemafile] = s;
		s->path = schemafile;
		s->is_loaded = true;
	}
	else {
		delete s;
		s = nullptr;
		cached_files[schemafile] = nullptr;
	}
	return s;
}

bool Schema::init()
{
	default_schema_obj = create_entity_from_properties_internal();
	return default_schema_obj != nullptr;
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