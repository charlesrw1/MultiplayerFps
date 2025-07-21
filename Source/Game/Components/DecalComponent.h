#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"

GENERATED_CLASS_INCLUDE("Render/MaterialPublic.h");

class MaterialInstance;
struct Render_Decal;
class DecalComponent : public Component
{
public:
	CLASS_BODY(DecalComponent);

	~DecalComponent();
	DecalComponent();

	void start() final;
	void stop() final;
	void on_changed_transform() final;
	void editor_on_change_property() final;
	void on_sync_render_data() final;

	REF void set_material(const MaterialInstance* mat);

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/decal.png";
	}
	std::unique_ptr<IComponentEditorUi> create_editor_ui() final;
#endif
private:
	REFLECT();
	AssetPtr<MaterialInstance> material;
	handle<Render_Decal> handle;
};
