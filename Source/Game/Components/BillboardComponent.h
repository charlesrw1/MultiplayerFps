#pragma once
#include "Game/EntityComponent.h"

#include <memory>
#include "Render/DynamicMaterialPtr.h"
#include "Render/Texture.h"
#include <unordered_map>
class Texture;
class MaterialInstance;
struct Render_Object;

// simple dynamic material billboard cache for texture overrides
class BillboardMaterialCache
{
public:
	static BillboardMaterialCache& get() {
		static BillboardMaterialCache cache;
		return cache;
	}
	MaterialInstance* find(Texture* t);

private:
	std::unordered_map<Texture*, MaterialInstance*> texture_to_mat;
};

class BillboardComponent : public Component
{
public:
	CLASS_BODY(BillboardComponent);

	BillboardComponent();
	~BillboardComponent();

	void start() final;
	void stop() final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
#endif
	void on_changed_transform() final;
	void on_sync_render_data() final;
	void set_texture(const Texture* tex);
	void set_is_visible(bool b) {
		if (visible != b) {
			visible = b;
			sync_render_data();
		}
	}
	bool get_is_visible() const { return visible; }

private:
	REF bool visible = true;
	MaterialInstance* dynamic_mat = nullptr;
	REF AssetPtr<Texture> texture;
	handle<Render_Object> handle;
};