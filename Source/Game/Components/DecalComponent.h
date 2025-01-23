#pragma once
#include "Game/EntityComponent.h"

class MaterialInstance;
struct Render_Decal;
CLASS_H(DecalComponent, EntityComponent)
public:
	~DecalComponent();
	DecalComponent();

	void start() override;
	void end() override;
	void on_changed_transform() override;
	void editor_on_change_property() override;
	static const PropertyInfoList* get_props();

	void set_material(const MaterialInstance* mat);
private:
	void update_handle();
	AssetPtr<MaterialInstance> material;
	handle<Render_Decal> handle;
};
