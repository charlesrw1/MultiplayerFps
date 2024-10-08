#include "Level.h"
#include "Render/Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Render/Texture.h"
#include <array>
#include "Physics/Physics2.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetRegistry.h"
#include "Game/Schema.h"
#include "Assets/AssetDatabase.h"

extern IEditorTool* g_editor_doc;
class MapAssetMetadata : public AssetMetadata
{
public:
	MapAssetMetadata() {
		extensions.push_back("tmap");
		extensions.push_back("bmap");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return { 185, 235, 237 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Map";
	}

	virtual bool assets_are_filepaths()  const { return true; }

	virtual IEditorTool* tool_to_edit_me() const { return g_editor_doc; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Level::StaticType; }
};
static AutoRegisterAsset<MapAssetMetadata> map_register_0987;




void Physics_Mesh::build()
{
	std::vector<Bounds> bound_vec;
	for (int i = 0; i < tris.size(); i++) {
		Physics_Triangle& tri = tris[i];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = verticies[tri.indicies[i]];
		Bounds b(corners[0]);
		b = bounds_union(b, corners[1]);
		b = bounds_union(b, corners[2]);
		b.bmin -= glm::vec3(0.01);
		b.bmax += glm::vec3(0.01);

		bound_vec.push_back(b);
	}

	float time_start = GetTime();
	bvh = BVH::build(bound_vec, 1, BVH_SAH);
	printf("Built bvh in %.2f seconds\n", (float)GetTime() - time_start);
}



#include "Framework/Files.h"
#include "Framework/DictWriter.h"
#include <fstream>
CLASS_IMPL(Level);


Level::Level() : all_world_ents(4/*2^4*/), tick_list(4)
{

}
Level::~Level()
{
	//for (auto ent : all_world_ents) {
	//	ent->destroy();
	//	delete ent;
	//}
	//all_world_ents.clear_all();
}

Level* LevelSerialization::create_empty_level(const std::string& file, bool is_editor)
{
	Level* l = new Level;
	l->bIsEditorLevel = is_editor;


	GetAssets().install_system_asset(l, file);

	return l;
}

Level* LevelSerialization::unserialize_level(const std::string& file, bool is_editor)
{
	auto fileptr  = FileSys::open_read_game(file.c_str());
	if (!fileptr) {
		sys_print("!!! couldn't open level %s\n", file.c_str());
		return nullptr;
	}
	std::string str(fileptr->size(), ' ');
	fileptr->read((void*)str.data(), str.size());
	auto ents = LevelSerialization::unserialize_entities_from_string(str);

	Level* level = new Level;
	level->bIsEditorLevel = is_editor;
	for (auto ent : ents) {
		if (ent) {
			level->all_world_ents.insert(ent->self_id.handle, ent);
			level->last_id = glm::max(ent->self_id.handle, level->last_id);
		}
	}

	return level;
}


bool Level::load_asset(ClassBase*&)
{
	auto& path = get_name();

	auto fileptr = FileSys::open_read_game(path.c_str());
	if (!fileptr) {
		printf("!!! couldn't open level %s\n", path.c_str());
		return false;
	}
	std::string str(fileptr->size(), ' ');
	fileptr->read((void*)str.data(), str.size());
	auto ents = LevelSerialization::unserialize_entities_from_string(str);

	for (auto ent : ents) {
		if (ent) {
			all_world_ents.insert(ent->self_id.handle, ent);
			last_id = glm::max(ent->self_id.handle, last_id);
		}
	}

	return true;
}

void Level::remove_from_update_list(BaseUpdater* b) {
	tick_list.remove(b);
	for(int i=0;i<wantsToAddToUpdate.size();i++)
		if (wantsToAddToUpdate[i] == b) {
			wantsToAddToUpdate[i] = nullptr;
		}
}


std::string LevelSerialization::serialize_entities_to_string(const std::vector<Entity*>& entities)
{
	SerializeEntityObjectContext ctx;
	for (int i = 0; i < entities.size(); i++)
		ctx.to_serialize_index[entities[i]] = i;

	DictWriter out;
	for (int i = 0; i < entities.size(); i++)
		serialize_one_entity(entities[i], out, ctx);

	return out.get_output();
}

#if 0
static std::pair<StringView,bool> read_just_props(ClassBase* e, DictParser& parser, SerializeEntityObjectContext* ctx)
{
	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &e->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, e });
		typeinfo = typeinfo->super_typeinfo;
	}

	return read_multi_properties(props, parser, {}, ctx);
}
#endif

bool LevelSerialization::unserialize_one_item(StringView tok, DictParser& in, SerializeEntityObjectContext& ctx)
{
	if (!in.check_item_start(tok)) 
		return false;
	in.read_string(tok);

	const bool is_class = tok.cmp("class");
	if (is_class || tok.cmp("schema")) {

		const ClassTypeInfo* typeinfo{};
		Entity* e = nullptr;
		if (is_class) {
			in.read_string(tok);
			typeinfo = ClassBase::find_class(tok.to_stack_string().c_str());
			if (!typeinfo || !typeinfo->allocate || !typeinfo->is_a(Entity::StaticType)) {
				sys_print("!!! no typeinfo to spawn level serialization %s\n", tok.to_stack_string().c_str());
				return false;
			}
			e = (Entity*)typeinfo->allocate();
		}
		else {
			in.read_string(tok);
			auto schematype = GetAssets().find_sync<Schema>(std::string(tok.str_start,tok.str_len));
			if (!schematype) {
				sys_print("!!! no schematype to spawn level serialization %s\n", tok.to_stack_string().c_str());
				return false;
			}
			e = schematype->create_entity_from_properties();
			typeinfo = &e->get_type();
		}
		//auto res = read_just_props(e,in,&ctx);
		auto res = read_props_to_object(e, typeinfo, in, {}, &ctx);

		bool good = res.second;
		tok = res.first;
		if (!good) {
			delete e;
			return false;
		}
		else
			ctx.unserialized.push_back(e);
	}
	else {
		const bool is_component_override = tok.cmp("component_override");
		if (is_component_override || tok.cmp("component_instance")) {
			EntityComponent* ec = nullptr;
			const ClassTypeInfo* ec_typeinfo = nullptr;
			int integer{};
			in.read_int(integer);
			if (integer < 0)integer = 0xfffffff;
			in.read_string(tok);
			auto classname_or_varname = tok.to_stack_string();

			if (integer > ctx.unserialized.size()) {
				sys_print("!!! comp_X parent integer greater than unserialized so far\n");
				return false;
			}
			Entity* parent = ctx.unserialized.at(integer);

			ctx.entity_serialzing = parent;

			if (is_component_override) {
				ec = parent->find_component_for_string_name(classname_or_varname.c_str());
				if (!ec) {
					sys_print("!!! no entitycmp for var name existing %s\n", classname_or_varname.c_str());
					return false;
				}
				ec_typeinfo = &ec->get_type();
			}
			else {
				ec_typeinfo = ClassBase::find_class(classname_or_varname.c_str());
				if (!ec_typeinfo || !ec_typeinfo->is_a(EntityComponent::StaticType) || !ec_typeinfo->allocate) {
					sys_print("!!! no entitycmp for type info to instance %s\n", classname_or_varname.c_str());
					return false;
				}
				ec = (EntityComponent*)ec_typeinfo->allocate();
			}
		
			auto res = read_props_to_object(ec, ec_typeinfo, in, {}, &ctx);
			//auto res = read_just_props(ec, in, &ctx);
			bool good = res.second;
			tok = res.first;
			if (!good) {
				delete ec;
				return false;
			}
			else {
				if (!is_component_override) {
					ec->post_unserialize_created_component(parent);
				}
			}
		}
		else {
			sys_print("!!! unknown item type on level serialization %s\n", tok.to_stack_string().c_str());
			return false;
		}
	}
	
	if (!in.check_item_end(tok)) 
		return false;
	return true;
}

std::vector<Entity*> LevelSerialization::unserialize_entities_from_string(const std::string& str)
{
	DictParser in;
	in.load_from_memory((uint8_t*)str.data(), str.size(), "lvlsrl");
	SerializeEntityObjectContext ctx;
	StringView tok;
	while (in.read_string(tok) && !in.is_eof()) {
		unserialize_one_item(tok, in, ctx);
	}
	return std::move(ctx.unserialized);
}
static void write_just_props(ClassBase* e, const ClassBase* diff, DictWriter& out, SerializeEntityObjectContext* ctx)
{
	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &e->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, e });
		typeinfo = typeinfo->super_typeinfo;
	}

	for (auto& proplist : props) {
		if (proplist.list)
			write_properties_with_diff(*const_cast<PropertyInfoList*>(proplist.list), e, diff, out, ctx);
	}
}
void LevelSerialization::serialize_one_entity(Entity* e, DictWriter& out, SerializeEntityObjectContext& ctx)
{
	ctx.entity_serialzing = e;

	const int myindex = ctx.to_serialize_index.find((ClassBase*)e)->second;

	const Entity* diffclass = (e->schema_type.get()) ? e->schema_type->get_default_schema_obj() : (Entity*)e->get_type().default_class_object;

	assert(e->get_type() == diffclass->get_type());

	out.write_item_start();
	if (e->schema_type.get()) {
		out.write_key_value("schema", e->schema_type->get_name().c_str());
	}
	else
		out.write_key_value("class", e->get_type().classname);

	write_just_props(e, diffclass, out, &ctx);

	out.write_item_end();

	for (int i = 0; i < e->all_components.size(); i++) {
		auto& c = e->all_components[i];

		if (c->dont_serialize_or_edit_this())
			continue;

		out.write_item_start();

		EntityComponent* ec = nullptr;

		ec = diffclass->find_component_for_string_name(c->eSelfNameString);
		const bool is_owned = ec == nullptr || !ec->is_native_componenent;
		if (is_owned) {
			out.write_key("component_instance");
			out.write_value(string_format("%d", myindex));
			out.write_value(c->get_type().classname);
			auto type = c->get_type().default_class_object;
			write_just_props(c.get(), type, out, &ctx);
		}
		else {
			out.write_key("component_override");
			out.write_value(string_format("%d",myindex));
			out.write_value(c->eSelfNameString.c_str());
			write_just_props(c.get(), ec, out, &ctx);
		}

		out.write_item_end();
	}
}

void Level::insert_entity_into_hashmap(Entity* e) {
	ASSERT(e);
	ASSERT(!e->self_id.is_valid());
	e->self_id.handle = get_next_id_and_increment();
	ASSERT(all_world_ents.find(e->self_id.handle) == nullptr);

	if (b_in_in_level_initialize.get_value()) {
		deferredSpawnList.push_back(e);
	}
	else
		all_world_ents.insert(e->self_id.handle, e);

}

void Level::init_entities_post_load()
{
	{
		BooleanScope scope(b_is_in_update_tick);
		// after inserting everything, call spawn functions
		for (auto ent : all_world_ents) {
			ent->initialize();
		}
	}
	for (auto wantToAdd : deferredSpawnList) {
		if (!wantToAdd)
			continue;
		if (all_world_ents.find(wantToAdd->self_id.handle)) {
			sys_print("!!! insert_unserialized_entities_into_level without assign_new_ids: bad handle\n");
			ASSERT(0);
			delete wantToAdd;
			continue;
		}
		all_world_ents.insert(wantToAdd->self_id.handle, wantToAdd);
	}
	deferredSpawnList.clear();
}

void Level::insert_unserialized_entities_into_level(std::vector<Entity*> ents, bool assign_new_ids)
{
	for (auto ent : ents) {

		if (assign_new_ids)
			ent->self_id.handle = get_next_id_and_increment();

		if (all_world_ents.find(ent->self_id.handle)) {
			sys_print("!!! insert_unserialized_entities_into_level without assign_new_ids: bad handle\n");
			ASSERT(0);
			delete ent;
			continue;
		}
		all_world_ents.insert(ent->self_id.handle, ent);

		ent->initialize();
	}
}

std::string LevelSerialization::serialize_level(Level* l)
{
	std::vector<Entity*> all_ents;
	for (auto ent : l->all_world_ents)
		all_ents.push_back(ent);
	return serialize_entities_to_string(all_ents);
}


enum class SerializeTypeHeader
{
	Schema,
	Class,
	ComponentInstance,
	ComponentOverride
};
#include "Framework/BinaryReadWrite.h"
#include "Framework/SerializerFunctions.h"
static void write_just_props_binary(ClassBase* e, const ClassBase* diff, FileWriter& out, SerializeEntityObjectContext* ctx)
{
	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &e->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, e });
		typeinfo = typeinfo->super_typeinfo;
	}

	for (auto& proplist : props) {
		if (proplist.list)
			write_properties_with_diff_binary(*const_cast<PropertyInfoList*>(proplist.list), e, diff, out, ctx);
	}
	out.write_string("");
}
void LevelSerialization::serialize_one_entity_binary(Entity* e, FileWriter& out, SerializeEntityObjectContext& ctx)
{
	ctx.entity_serialzing = e;

	const int myindex = ctx.to_serialize_index.find((ClassBase*)e)->second;

	const Entity* diffclass = (e->schema_type.get()) ? e->schema_type->get_default_schema_obj() : (Entity*)e->get_type().default_class_object;

	assert(e->get_type() == diffclass->get_type());

	if (e->schema_type.get()) {
		out.write_byte((int)SerializeTypeHeader::Schema);
		out.write_string(e->schema_type->get_name());
	}
	else {
		out.write_byte((int)SerializeTypeHeader::Class);
		out.write_string(e->get_type().classname);
	}

	write_just_props_binary(e, diffclass, out, &ctx);


	for (int i = 0; i < e->all_components.size(); i++) {


		auto& c = e->all_components[i];
		EntityComponent* ec = nullptr;

		ec = diffclass->find_component_for_string_name(c->eSelfNameString);
		const bool is_owned = ec == nullptr || !ec->is_native_componenent;
		if (is_owned) {

			out.write_byte((int)SerializeTypeHeader::ComponentInstance);
			out.write_int32(myindex);
			out.write_string(c->get_type().classname);

			auto type = c->get_type().default_class_object;
			write_just_props_binary(c.get(), type, out, &ctx);
		}
		else {
			out.write_byte((int)SerializeTypeHeader::ComponentOverride);
			out.write_int32(myindex);
			out.write_string(c->eSelfNameString.c_str());

			write_just_props_binary(c.get(), ec, out, &ctx);
		}
	}
}
void LevelSerialization::serialize_level_binary(Level* l, FileWriter& out)
{
	std::vector<Entity*> all_ents;
	for (auto ent : l->all_world_ents)
		all_ents.push_back(ent);

	SerializeEntityObjectContext ctx;
	for (int i = 0; i < all_ents.size(); i++)
		ctx.to_serialize_index[all_ents[i]] = i;

	for (int i = 0; i < all_ents.size(); i++)
		serialize_one_entity_binary(all_ents[i], out, ctx);
}

bool LevelSerialization::unserialize_one_item_binary(BinaryReader& in, SerializeEntityObjectContext& ctx)
{
	
	SerializeTypeHeader header_type = (SerializeTypeHeader)in.read_byte();
	switch (header_type)
	{
	case SerializeTypeHeader::Class: {
		auto classname = in.read_string_view();
		auto str = std::string(classname.str_start, classname.str_len);
		auto typeinfo = ClassBase::find_class(str.c_str());
		if (!typeinfo || !typeinfo->allocate || !typeinfo->is_a(Entity::StaticType)) {
			sys_print("!!! no typeinfo to spawn level serialization %s\n", str.c_str());
			return false;
		}
		Entity* e = (Entity*)typeinfo->allocate();

		read_props_to_object_binary(e, typeinfo, in, &ctx);
		ctx.unserialized.push_back(e);
	}
		break;
	case SerializeTypeHeader::Schema: {
		auto schemaname = in.read_string_view();
		auto str = std::string(schemaname.str_start, schemaname.str_len);
		auto schematype = GetAssets().find_sync<Schema>(str);
		if (!schematype) {
			sys_print("!!! no schematype to spawn level serialization %s\n", str.c_str());
			return false;
		}
		Entity* e = schematype->create_entity_from_properties();
		auto typeinfo = &e->get_type();
		read_props_to_object_binary(e, typeinfo, in, &ctx);
		ctx.unserialized.push_back(e);
	}
		break;
	case SerializeTypeHeader::ComponentInstance:
	case SerializeTypeHeader::ComponentOverride: {
		uint32_t integer = in.read_int32();
		auto classname_or_varname = in.read_string_view();
		auto str = std::string(classname_or_varname.str_start, classname_or_varname.str_len);

		if (integer >= ctx.unserialized.size()) {
			throw std::runtime_error("!!! comp_X parent integer greater than unserialized so far");
		}
		Entity* parent = ctx.unserialized.at(integer);
		ctx.entity_serialzing = parent;

		if (header_type == SerializeTypeHeader::ComponentOverride) {
			EntityComponent* ec = parent->find_component_for_string_name(str.c_str());
			if (!ec) {
				throw std::runtime_error("!!! no entitycmp for var name existing");
			}
			const ClassTypeInfo* ec_typeinfo = &ec->get_type();


			read_props_to_object_binary(ec, ec_typeinfo, in, &ctx);

		}
		else {
			const ClassTypeInfo*  ec_typeinfo = ClassBase::find_class(str.c_str());
			if (!ec_typeinfo || !ec_typeinfo->is_a(EntityComponent::StaticType) || !ec_typeinfo->allocate) {
				throw std::runtime_error("!!! no entitycmp for type info to instance");
			}
			auto ec = (EntityComponent*)ec_typeinfo->allocate();

			read_props_to_object_binary(ec, ec_typeinfo, in, &ctx);

			ec->post_unserialize_created_component(parent);
		}
	}break;
	default:
		throw std::runtime_error("bad header type schema/classname\n");

	}
	return true;
}
void LevelSerialization::unserialize_level_binary(Level* l, BinaryReader& writer)
{
	SerializeEntityObjectContext ctx;
	while (!writer.has_failed() && !writer.is_eof()) {
		unserialize_one_item_binary(writer, ctx);
	}
}

DECLARE_ENGINE_CMD(SLB)
{
	auto start = GetTime();

	auto lvl = eng->get_level();
	if (!lvl) return;
	FileWriter out;
	LevelSerialization::serialize_level_binary(lvl, out);

	auto diff = GetTime() - start;
	sys_print("TIME: %f\n", (float)diff);
	std::ofstream outfile("file.bin",std::ios::binary);
	outfile.write(out.get_buffer(), out.get_size());

}
DECLARE_ENGINE_CMD(LLB)
{
	auto start = GetTime();
	auto lvl = new Level;
	auto file = FileSys::open_read_game("file.bin");
	BinaryReader in(file.get());
	LevelSerialization::unserialize_level_binary(lvl, in);
	auto diff = GetTime() - start;
	sys_print("TIME: %f\n", (float)diff);
}