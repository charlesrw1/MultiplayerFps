#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
#include "Assets/AssetLoaderRegistry.h"
#include "Game/Entity.h"

#include "Level.h"

static const char* schema_base = "./Data/Schema/";
CLASS_IMPL(InlinePtrFixup);

CLASS_IMPL(Schema);
REGISTERASSETLOADER_MACRO(Schema, &g_schema_loader);

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