#pragma once
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include <memory>
#include "Render/DynamicMaterialPtr.h"
#include "Render/Texture.h"
class Texture;
class MaterialInstance;
struct Render_Object;
class BillboardComponent : public Component
{
public:
	CLASS_BODY(BillboardComponent);

	BillboardComponent();
	~BillboardComponent();

	void start() final;
	void end() final;
	void editor_on_change_property() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;

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
	REF bool visible = true;
	DynamicMatUniquePtr dynamicMaterial = nullptr;
	REF AssetPtr<Texture> texture;
	handle<Render_Object> handle;
};