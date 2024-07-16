#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
#include "Assets/AssetLoaderRegistry.h"
#include "Entity.h"
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
	
	bool is_valid = s->check_validity_of_file();
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

bool Schema::check_validity_of_file()
{
	bool is_valid = create_entity_from_properties_internal(true) != nullptr;
	return is_valid;
}

Entity* Schema::create_entity_from_properties_internal(bool just_check_valid)
{

	DictParser in;
	in.load_from_memory((uint8_t*)properties.data(), properties.size(),"");

	StringView tok;
	in.read_string(tok);
	if (!tok.cmp("TYPE"))
		return nullptr;
	in.read_string(tok);
	if (!tok.cmp("Schema"))
		return nullptr;
	if (!in.expect_list_start())
		return nullptr;
	std::vector<ClassBase*> objs;
	bool good = in.read_list_and_apply_functor([&](StringView tok)->bool {
		ClassBase* obj = read_object_properties<ClassBase>({}, in, tok);
		if (obj)
			objs.push_back(obj);
		return true;
		});
	if (!good) {
		for (auto obj : objs)
			delete obj;
		return nullptr;
	}

	// find the entity, should be the first in the list
	Entity* e = nullptr;
	for (int i = 0; i < objs.size(); i++) {
		if (objs[i]->is_a<Entity>()) {
			e = static_cast<Entity*>(objs[i]);
			objs[i] = nullptr;
			break;
		}
	}
	if (!e) {
		for (auto obj : objs)
			delete obj;
		return nullptr;
	}

	// okay this is epic, only called internally obviously
	if (just_check_valid) {
		for (auto obj : objs)
			delete obj;
		return (Entity*)(1);
	}

	// now add the components
	for (int i = 0; i < objs.size(); i++) {
		if (!objs[i])
			continue;
		if (objs[i]->is_a<EntityComponent>()) {
			// to be explicit: entity takes ownership
			auto obj = objs[i];
			std::unique_ptr<EntityComponent> ptr(static_cast<EntityComponent*>(objs[i]));
			auto props = EntityComponent::StaticType.props;
			
			// fixup internal references
			for (int p = 0; p < props->count; p++) {
				if (strcmp(props->list[p].custom_type_str, "ObjectPointer") == 0) {
					// get serialized integer
					void** ptr_dest = (void**)props->list[p].get_ptr(obj);
					uintptr_t index = *(uintptr_t*)ptr_dest;

					if (index >= objs.size()) {
						sys_print("!!! bad ptr index on schema load\n");
						*ptr_dest = nullptr;
					}
					else {
						auto refed_obj = objs[index];

						// if if if if ...
						if (refed_obj->is_a<InlinePtrFixup>()) {
							InlinePtrFixup* ptr_to_ptr = (InlinePtrFixup*)refed_obj;

							auto the_actual_refed_object = objs[ptr_to_ptr->object_index];
							auto thelineline_prop = the_actual_refed_object->get_type().props->find(ptr_to_ptr->property_name.c_str());
							if (thelineline_prop && strcmp(thelineline_prop->custom_type_str, props->list[p].range_hint) == 0) {
								*ptr_dest = thelineline_prop->get_ptr(the_actual_refed_object);
							}
							else {
								sys_print("!!! couldnt find valid property for InlinePtrFixup\n");
								*ptr_dest = nullptr;
							}
						}
						else {
							if (refed_obj && strcmp(refed_obj->get_type().classname, props->list[p].range_hint) == 0) {
								*ptr_dest = refed_obj;
							}
							else {
								sys_print("!!! couldnt find direct reference\n");
								*ptr_dest = nullptr;
							}
						}
					}
				}
			}
			e->dynamic_components.push_back(std::move(ptr));
		}
		else if (!objs[i]->is_a<InlinePtrFixup>()) {
			sys_print("!!! unknown type got into schema file\n");
			delete objs[i];
		}
	}
	for (int i = 0; i < objs.size(); i++)
		if (objs[i]->is_a<InlinePtrFixup>())
			delete objs[i];

	return e;

}

void Schema::write_to_file(Entity* e)
{
	SerializeEntityObjectContext ctx;
	ctx.serialzing_entity_index = 0;
	ctx.to_serialize_index[e] = 0;

	const ClassBase* diffclass = (e->schema_type.get()) ? e->schema_type->default_schema_obj : e->get_type().default_class_object;

	assert(e->get_type() == diffclass->get_type());

	DictWriter out;
	out.write_item_start();
	if (e->schema_type.get()) {
		out.write_key_value("schema", e->schema_type->get_name().c_str());
	}
	else
		out.write_key_value("class", e->get_type().classname);

	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &e->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, e });
		typeinfo = typeinfo->super_typeinfo;
	}

	for (auto& proplist : props) {
		if (proplist.list)
			write_properties(*const_cast<PropertyInfoList*>(proplist.list), proplist.instance, out, &ctx);
	}

	out.write_item_end();

}

inline void write_object_properties_with_diff(
	ClassBase* obj,
	const ClassBase* diffed_obj,	/* obj.type == diffed_obj.type ALWAYS */
	ClassBase* userptr,
	DictWriter& out
)
{
	assert(obj->get_type() == diffed_obj->get_type());

	// if obj != diffed_obj
	//		

	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &obj->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, obj });
		typeinfo = typeinfo->super_typeinfo;
	}

	typeinfo = &obj->get_type();

	out.write_item_start();

	out.write_key_value("type", typeinfo->classname);
	for (auto& proplist : props) {
		if (proplist.list)
			write_properties(*const_cast<PropertyInfoList*>(proplist.list), proplist.instance, out, userptr);
	}

	out.write_item_end();
}
