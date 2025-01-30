#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
class Texture;
class MaterialInstance;
struct Render_Object;
CLASS_H(BillboardComponent,EntityComponent)
public:
	BillboardComponent();
	~BillboardComponent();

	void start() override;
	void end() override;
	void editor_on_change_property() override;
	void on_changed_transform() override;
	static const PropertyInfoList* get_props();

	void set_texture(const Texture* tex);

private:
	void fill_out_render_obj(Render_Object& obj);
	bool visible = true;
	MaterialInstance* dynamicMaterial = nullptr;
	AssetPtr<Texture> texture;
	handle<Render_Object> handle;
};