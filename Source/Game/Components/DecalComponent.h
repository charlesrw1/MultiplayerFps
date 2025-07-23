#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "MeshbuilderComponent.h"

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
	void on_sync_render_data() final;

	REF void set_material(MaterialInstance* mat);


#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/decal.png";
	}
#endif
private:
	REFLECT();
	AssetPtr<MaterialInstance> material;
	handle<Render_Decal> handle;
};
