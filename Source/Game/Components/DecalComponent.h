#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"

GENERATED_CLASS_INCLUDE("Render/MaterialPublic.h");

class MaterialInstance;
struct Render_Decal;
NEWCLASS(DecalComponent, EntityComponent)
public:
	~DecalComponent();
	DecalComponent();

	void start() final;
	void end() final;
	void on_changed_transform() final;
	void editor_on_change_property() final;
	void on_sync_render_data() final;

	void set_material(const MaterialInstance* mat);
private:
	REFLECT();
	AssetPtr<MaterialInstance> material;
	handle<Render_Decal> handle;
};
