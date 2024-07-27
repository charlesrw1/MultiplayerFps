#include "Level.h"
#include "Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>

#include "Physics/Physics2.h"

static const char* const maps_directory = "./Data/Maps/";
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "AssetRegistry.h"
#include "EditorDocPublic.h"

#include "Game/Schema.h"

class MapAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return { 185, 235, 237 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Map";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override
	{
		auto tree = FileSys::find_files("./Data/Maps",true);
		for (auto file : tree) {
			filepaths.push_back((file.substr(12)));
		}
	}
	virtual bool assets_are_filepaths()  const { return true; }
	virtual std::string root_filepath()  const override
	{
		return "./Data/Maps/";
	}
	virtual IEditorTool* tool_to_edit_me() const { return g_editor_doc; }
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



void make_static_mesh_from_dict(vector<handle<Render_Object>>& objs, vector<PhysicsActor*>& phys, const Dict& dict)
{
	const char* get_str = "";

	const char* modelname = dict.get_string("model");
	if (*modelname == 0) {
		return;
	}
	Model* model = mods.find_or_load(modelname);
	if (!model)
		return;

	glm::vec3 p = dict.get_vec3("position");
	glm::vec3 s = dict.get_vec3("scale",glm::vec3(1.f));
	glm::vec4 qvec = dict.get_vec4("rotation");
	glm::quat q = glm::quat(qvec.x, qvec.y, qvec.z, qvec.w);
	glm::mat4 transform = glm::translate(glm::mat4(1), p);
	transform *= glm::mat4_cast(q);
	transform = glm::scale(transform, s);

	Color32 color = dict.get_color("color");

	auto handle = idraw->get_scene()->register_obj();
	Render_Object rop;
	rop.param1 = color;
	rop.model = model;
	rop.transform = transform;
	rop.animator = nullptr;
	rop.visible = true;
	rop.shadow_caster = dict.get_int("casts_shadows", 1);

	bool has_collisions = dict.get_int("has_collisions", 1);
	idraw->get_scene()->update_obj(handle, rop);
	objs.push_back(handle);

	if (has_collisions) {
		// create physics
		PhysicsActor* actor = g_physics->allocate_physics_actor();
		assert(actor);
		actor->set_entity(nullptr);

		PhysTransform pt;
		pt.position = p;
		pt.rotation = q;

		glm::vec3 halfsize = (model->get_bounds().bmax - model->get_bounds().bmin) * 0.5f * glm::abs(s);
		glm::vec3 center = model->get_bounds().get_center() * s;
		center = glm::mat4_cast(q) * glm::vec4(center, 1.0);
		PhysTransform tr;
		tr.position = p;	// fixme: scaling for models
		tr.rotation = q;
		if (model->get_physics_body())
			actor->create_static_actor_from_model(model, tr);
		else
			actor->create_static_actor_from_shape(physics_shape_def::create_box(halfsize, p + center, q));
	
		phys.push_back(actor);
	}
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
	for (auto ent : all_world_ents) {
		ent->destroy();
		delete ent;
	}
	all_world_ents.clear_all();
}

Level* LevelSerialization::create_empty_level(const std::string& file, bool is_editor)
{
	Level* l = new Level;
	l->path = file;
	l->bIsEditorLevel = is_editor;
	return l;
}

Level* LevelSerialization::unserialize_level(const std::string& file, bool is_editor)
{
	auto fileptr  = FileSys::open_read(file.c_str());
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

bool LevelSerialization::unserialize_one_item(StringView tok, DictParser& in, SerializeEntityObjectContext& ctx)
{
	if (!in.check_item_start(tok)) return false;
	in.read_string(tok);

	const bool is_class = tok.cmp("class");
	if (is_class || tok.cmp("schema")) {

		Entity* e = nullptr;
		if (is_class) {
			in.read_string(tok);
			auto typeinfo = ClassBase::find_class(tok.to_stack_string().c_str());
			if (!typeinfo || !typeinfo->allocate || !typeinfo->is_a(Entity::StaticType)) {
				sys_print("!!! no typeinfo to spawn level serialization %s\n", tok.to_stack_string().c_str());
				return false;
			}
			e = (Entity*)typeinfo->allocate();
		}
		else {
			in.read_string(tok);
			auto schematype = g_schema_loader.load_schema(tok.to_stack_string().c_str());
			if (!schematype) {
				sys_print("!!! no schematype to spawn level serialization %s\n", tok.to_stack_string().c_str());
				return false;
			}
			e = schematype->create_entity_from_properties();
		}
		auto res = read_just_props(e,in,&ctx);
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
			}
			else {
				auto typeinfo = ClassBase::find_class(classname_or_varname.c_str());
				if (!typeinfo || !typeinfo->is_a(EntityComponent::StaticType) || !typeinfo->allocate) {
					sys_print("!!! no entitycmp for type info to instance %s\n", classname_or_varname.c_str());
					return false;
				}
				ec = (EntityComponent*)typeinfo->allocate();
			}
		

			auto res = read_just_props(ec, in, &ctx);
			bool good = res.second;
			tok = res.first;
			if (!good) {
				delete ec;
				return false;
			}
			else {
				if (!is_component_override)
					parent->all_components.push_back(std::unique_ptr<EntityComponent>(ec));
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

		out.write_item_start();

		auto& c = e->all_components[i];
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

void Level::init_entities_post_load()
{
	// after inserting everything, call spawn functions
	for (auto ent : all_world_ents) {
		ent->initialize();
	}
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