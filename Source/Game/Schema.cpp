#include "Game/Schema.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Framework/ObjectSerialization.h"
static const char* schema_base = "./Data/Schema/";
CLASS_IMPL(InlinePtrFixup);

Entity* SchemaLoader::create_but_dont_instantiate_schema(const std::string& schemafile)
{
	std::string path = schema_base + schemafile;
	auto file = FileSys::open_read(path.c_str());
	if (!file)
		return nullptr;
	DictParser in;
	in.load_from_file(file.get());

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

	// find the entity, should be the first in the list
	Entity* e = nullptr;
	for (int i = 0; i < objs.size(); i++) {
		if (objs[i]->is_a<Entity>()) {
			e = static_cast<Entity*>(objs[i]);
			objs[i] = nullptr;
			break;
		}
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

			e->all_components.push_back(ptr.get());
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