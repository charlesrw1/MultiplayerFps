#pragma once
#include "Game/EntityComponent.h"

class Texture;
class MaterialInstance;
struct Render_Object;
CLASS_H(BillboardComponent,EntityComponent)
public:
	BillboardComponent();
	~BillboardComponent();

	void on_init() override;
	void on_deinit() override;
	void editor_on_change_property() override;
	void on_changed_transform() override;
	static const PropertyInfoList* get_props() {
		START_PROPS(BillboardComponent)
			REG_ASSET_PTR(texture, PROP_DEFAULT),
			REG_BOOL(visible,PROP_DEFAULT,"1"),
		END_PROPS(BillboardComponent)
	}

	void set_texture(const Texture* tex);

private:
	void fill_out_render_obj(Render_Object& obj);
	bool visible = true;
	MaterialInstance* dynamicMaterial = nullptr;
	AssetPtr<Texture> texture;
	handle<Render_Object> handle;
};