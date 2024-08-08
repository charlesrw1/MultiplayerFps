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
		return "Entity (C++)";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override {
		auto subclasses = ClassBase::get_subclasses<Entity>();
		for (; !subclasses.is_end(); subclasses.next()) {
			if (subclasses.get_type()->allocate)
				filepaths.push_back(subclasses.get_type()->classname);
		}
	}
	virtual IEditorTool* tool_to_edit_me() const override { return g_editor_doc; }
	virtual std::string root_filepath() const  override
	{
		return "";
	}
	virtual bool assets_are_filepaths() const { return false; }
};
REGISTER_ASSETMETADATA_MACRO(EntityTypeMetadata);



// database to map an integer to any type of object, for example models or entities, automatically resolved and editable in the editor

const PropertyInfoList* Entity::get_props() {
	START_PROPS(Entity)
		REG_ENTITY_PTR(self_id,PROP_SERIALIZE),
		REG_STDSTRING(editor_name, PROP_DEFAULT)	// edit+serialize
	END_PROPS(Entity)
}

Entity::Entity()
{
}

void Entity::initialize()
{
	if (!root_component.get()) {

		// try to find a root if it wasnt set by now
		for (int i = 0; i < all_components.size(); i++) {
			if (all_components[i]->is_force_root) {
				root_component.ptr = all_components[i].get();
				break;
			}
		}
		// couldnt find root, create an empty one
		if (!root_component.get()) {
			root_component = create_sub_component<EmptyComponent>("DefaultRoot");
			root_component->is_native_componenent = false;
			root_component->is_force_root = true;
		}
	}


	// now run register functions
	// important!: store the number of components here before running init()s, some components might create components in their functions
	// and on_init shouldnt be run twice for them
	const size_t num_components_pre_init = all_components.size();
	for (int i = 0; i < num_components_pre_init; i++) {
		// insert logic for editor only components here
		auto& c = all_components[i];
		if (c->attached_parent.get() == nullptr && root_component.get() != c.get())
			c->attach_to_parent(root_component.get());
		c->on_init();
	}

	if (!eng->is_editor_level()) {
		start();
	}
}

void Entity::destroy()
{
	if (!eng->is_editor_level()) {
		end();
	}

	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->on_deinit();
	all_components.clear();	// deletes all
}


void Entity::update_entity_and_components() {
	// call the entity tick function
	update();

	// tick components, update renderables, animations etc.
	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->on_tick();
}

Entity::~Entity()
{


}
