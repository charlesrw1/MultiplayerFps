#include "Entity.h"

#include "Framework/Factory.h"
#include "Framework/ArrayReflection.h"
#include "Game/Schema.h"
#include "glm/gtx/euler_angles.hpp"
#include "GameEnginePublic.h"
#include "Assets/AssetRegistry.h"
#include "Level.h"

CLASS_IMPL(Entity);


// create native entities as a fake "Asset" for drag+drop and double click open to create instance abilities
extern IEditorTool* g_editor_doc;	// EditorDocPublic.h
class EntityTypeMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 117, 75, 242 };
	}

	virtual std::string get_type_name() const  override
	{
		return "EntityClass";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override {
		auto subclasses = ClassBase::get_subclasses<Entity>();
		for (; !subclasses.is_end(); subclasses.next()) {
			if (subclasses.get_type()->allocate) {
				std::string path = subclasses.get_type()->classname;
				auto parent = subclasses.get_type();
				while (parent && parent != &Entity::StaticType) {
					path.insert(0, std::string(parent->super_typeinfo->classname) + "/");
					parent = parent->super_typeinfo;
				}
				
				filepaths.push_back(path);
			}
		}
	}
	virtual IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }

	virtual bool assets_are_filepaths() const { return false; }
};
REGISTER_ASSETMETADATA_MACRO(EntityTypeMetadata);



// database to map an integer to any type of object, for example models or entities, automatically resolved and editable in the editor

const PropertyInfoList* Entity::get_props() {
	START_PROPS(Entity)
		REG_ENTITY_PTR(self_id,PROP_SERIALIZE),
		REG_STDSTRING(editor_name, PROP_DEFAULT),	// edit+serialize
		REG_ENTITY_PTR(parentedEntity, PROP_SERIALIZE)
	END_PROPS(Entity)
}

Entity::Entity()
{
}

void Entity::initialize()
{
	if (!root_component) {

		// try to find a root if it wasnt set by now
		for (int i = 0; i < all_components.size(); i++) {
			if (all_components[i]->is_force_root) {
				root_component = all_components[i].get();
				break;
			}
		}
		// couldnt find root, create an empty one
		if (!root_component) {
			root_component = create_sub_component<EmptyComponent>("DefaultRoot");
			root_component->is_native_componenent = false;
			root_component->is_force_root = true;
		}
	}

	// parented entity from the level
	if (parentedEntity) {
		root_component->attach_to_parent(parentedEntity->root_component);
		root_component->post_change_transform_R(true);	// just call this here
	}

	// now run register functions
	// important!: store the number of components here before running init()s, some components might create components in their functions
	// and on_init shouldnt be run twice for them
	const size_t num_components_pre_init = all_components.size();
	for (int i = 0; i < num_components_pre_init; i++) {
		// insert logic for editor only components here
		auto& c = all_components[i];
		if (c->attached_parent.get() == nullptr && root_component != c.get())
			c->attach_to_parent(root_component);
		c->init();
	}


	if (!eng->is_editor_level()) {
		start();
		init_updater();
	}
}

void Entity::destroy()
{
	if (!eng->is_editor_level()) {
		end();
		shutdown_updater();
	}

	assert(root_component);
	root_component->unlink_from_parent();	// unlink this from another parent entity potentially

	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->deinit(false /* dont destroy subcomponents, were doing that already*/);
	all_components.clear();	// deletes all
}

void Entity::parent_to_entity(Entity* other)
{
	parent_to_component((other)?other->root_component:nullptr);
}
void Entity::parent_to_component(EntityComponent* other)
{
	if (!other)
	{
		root_component->unlink_from_parent();
		return;
	}

	if (other->entity_owner == this) {
		sys_print("??? cant parent entity to its own component\n");
	}
	else {
		root_component->attach_to_parent(other);
		parentedEntity = other->entity_owner->self_id;
	}
}

void Entity::remove_this_component(EntityComponent* c)
{
	if (c == root_component) {
		sys_print("??? cant delete root component\n");
		return;
	}
	sys_print("*** removing this component %s\n", c->get_type().classname);
	assert(c->entity_owner == this);
	bool found = false;
	for (int i = 0; i < all_components.size(); i++) {
		if (all_components[i].get() == c) {
			c->deinit(true /* destroy sub components*/);
			all_components.erase(all_components.begin() + i);
			found = true;
			break;
		}
	}
	assert(found && "component not found in remove_this_component");

}

Entity::~Entity()
{


}
