#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include <memory>
#include "Render/DynamicMaterialPtr.h"

class Texture;
class MaterialInstance;
struct Render_Object;
CLASS_H(BillboardComponent,EntityComponent)	//fixme: NEWCLASS
public:
	BillboardComponent();
	~BillboardComponent();

	void start() final;
	void end() final;
	void editor_on_change_property() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;

	static const PropertyInfoList* get_props();

	void set_texture(const Texture* tex);
	
	void set_is_visible(bool b) {
		if (visible != b) {
			visible = b;
			sync_render_data();
		}
	}
	bool get_is_visible() const {
		return visible;
	}

private:
	bool visible = true;
	DynamicMatUniquePtr dynamicMaterial = nullptr;
	AssetPtr<Texture> texture;
	handle<Render_Object> handle;
};