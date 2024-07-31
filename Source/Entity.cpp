#include "Entity.h"

#include "EntityTypes.h"
#include "Framework/Factory.h"
#include "Physics/Physics2.h"

#include "Animation/Runtime/Animation.h"
#include "Render/MaterialPublic.h"

#include "Framework/ArrayReflection.h"
#include "Game/Schema.h"


#include "glm/gtx/euler_angles.hpp"

#include "GameEnginePublic.h"
#include "DrawPublic.h"

#include "AssetRegistry.h"

#include "Game/StdEntityTypes.h"

#include "Level.h"

CLASS_IMPL(Entity);

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);
CLASS_IMPL(StaticMeshEntity);
CLASS_IMPL(PrefabEntity);
CLASS_IMPL(PrefabSelection);
CLASS_IMPL(PointLightEntity);
CLASS_IMPL(SpotLightEntity);
CLASS_IMPL(SunLightEntity);



CLASS_IMPL(EntityComponent);

CLASS_IMPL(EmptyComponent);
CLASS_IMPL(MeshComponent);
CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);
CLASS_IMPL(PointLightComponent);
CLASS_IMPL(SpotLightComponent);
CLASS_IMPL(SunLightComponent);


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
		REG_ENTITY_COMPONENT_PTR(root_component, PROP_SERIALIZE),
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
		root_component = create_sub_component<EmptyComponent>("DefaultRoot");
		root_component->is_native_componenent = false;
	}


	// now run register functions
	for (int i = 0; i < all_components.size(); i++) {
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
void EntityComponent::remove_this(EntityComponent* child_component)
{
#ifdef _DEBUG
	bool found = false;
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child_component) {
			if (found)
				assert(!"component was added twice");
			children.erase(children.begin() + i);
			i--;
			found = true;
		}
	}
	assert(found && "component couldn't be found to remove in remove_this");
	return;
#else
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child_component) {
			children.erase(children.begin() + i);
			return;
		}
	}
	assert("component couldn't be found to remove in remove_this");
#endif
}

void EntityComponent::post_unserialize_created_component(Entity * parent)
{
	parent->add_component_from_loading(this);	// add the component to the list (doesnt initalize it yet)
	if (attached_parent.get())	// set the parent if it got serialized, might redo this to make it clearer
		attached_parent->children.push_back(this);
}

void EntityComponent::attach_to_parent(EntityComponent* parent_component, StringName point)
{
	ASSERT(parent_component);

	// prevents circular parent creations
	// checks the node we are parenting to's tree to see if THIS is one of the parent nodes
	EntityComponent* cur_node = parent_component;
	while (cur_node) {

		if (cur_node->get_parent_component() == this) {
			ASSERT(attached_parent.get());
			remove_this(cur_node);
			cur_node->attached_parent = {};
			cur_node->attach_to_parent(attached_parent.get());
			break;
		}
		cur_node = cur_node->attached_parent.get();
	}

	if (attached_parent.get()) {
		attached_parent->remove_this(this);
		attached_parent = nullptr;
	}
	parent_component->children.push_back(this);
	attached_parent = parent_component;
	attached_bone_name = point;

}
void EntityComponent::unlink_and_destroy() 
{
	if (attached_parent.get())
		attached_parent->remove_this(this);
	for (int i = 0; i < children.size(); i++)
		children[i]->destroy_children_no_unlink();
	on_deinit();
}
void EntityComponent::destroy_children_no_unlink()
{
	for (int i = 0; i < children.size(); i++)
		children[i]->destroy_children_no_unlink();
	on_deinit();
	entity_owner->remove_this_component(this);
}

#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"


MeshComponent::~MeshComponent()
{
	assert(!animator && !draw_handle.is_valid());
}
MeshComponent::MeshComponent() {}
void MeshComponent::set_model(const char* model_path)
{
	Model* modelnext = mods.find_or_load(model_path);
	if (modelnext != model.get()) {
		model = modelnext;
		on_changed_transform();	//fixme
	}
}
void MeshComponent::set_model(Model* modelnext)
{
	if (modelnext != model.get()) {
		model = modelnext;
		on_changed_transform();	//fixme
	}
}

const PropertyInfoList* MeshComponent::get_props() {
#ifndef RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance>, eMaterialOverride);
#endif // !RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance>, MaterialOverride_compilied)

		auto t = &Model::StaticType.classname;
		const char* str = Model::StaticType.classname;
	START_PROPS(MeshComponent)
		REG_ASSET_PTR(model, PROP_DEFAULT),
		REG_ASSET_PTR(animator_tree, PROP_DEFAULT),
		REG_BOOL(cast_shadows, PROP_DEFAULT, "1"),
#ifndef RUNTIME
		REG_STDVECTOR(eMaterialOverride, PROP_DEFAULT | PROP_EDITOR_ONLY),
		REG_BOOL(eAnimateInEditor, PROP_DEFAULT | PROP_EDITOR_ONLY, "0"),
#endif // !RUNTIME

		REG_BOOL(simulate_physics, PROP_DEFAULT, "0"),
	END_PROPS(MeshCompponent)
}

void MeshComponent::editor_on_change_property()
{
	update_handle();
}

void MeshComponent::update_handle()
{
	if (!model.get())
		return;

	Render_Object obj;
	obj.model = model.get();
	obj.visible = visible;
	obj.transform = get_ws_transform();
	obj.owner = this;
	obj.shadow_caster = cast_shadows;
	if (!eMaterialOverride.empty())
		obj.mat_override = eMaterialOverride[0].get();
	idraw->get_scene()->update_obj(draw_handle, obj);
}

void MeshComponent::on_init()
{
	draw_handle = idraw->get_scene()->register_obj();
	if (model.get()) {
		if (model->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {
			assert(animator_tree->get_script());
			assert(animator_tree->get_script()->get_native_class());
			assert(animator_tree->get_script()->get_native_class()->allocate);

			ClassBase* c = animator_tree->get_script()->get_native_class()->allocate();
			assert(c->is_a<AnimatorInstance>());
			animator.reset(c->cast_to<AnimatorInstance>());

			bool good = animator->initialize_animator(model.get(), animator_tree.get(), get_owner());
			if (!good) {
				sys_print("!!! couldnt initialize animator\n");
				animator.reset(nullptr);	// free animator
				animator_tree = nullptr;	// free tree reference
			}
		}

		Render_Object obj;
		obj.model = model.get();
		obj.visible = visible;
		obj.transform = get_ws_transform();
		obj.owner = this;
		obj.shadow_caster = cast_shadows;

		if (!eMaterialOverride.empty())
			obj.mat_override = eMaterialOverride[0].get();

		idraw->get_scene()->update_obj(draw_handle, obj);
	}
}

void MeshComponent::on_changed_transform()
{
	update_handle();
}

void MeshComponent::on_tick()
{

}

void MeshComponent::on_deinit()
{
	idraw->get_scene()->remove_obj(draw_handle);
	animator.reset();
}

Entity::~Entity()
{


}
