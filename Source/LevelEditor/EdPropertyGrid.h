#pragma once
#include "AllHeader.h"
#include "Framework/PropertyEd.h"
class ISelectionApi;
class EdPropertyGrid
{
public:
	EdPropertyGrid(const FnFactory<IPropertyEditor>& factory);
	void draw(const ISelectionApi& api);

	viewMulticastDelegate<> get_on_property_changed() { return on_property_change_internal; }

	// Event callbacks
	void refresh_grid(const ISelectionApi& api);

	bool property_grid_has_rows() const { return grid_cache.row_count() > 0; }
private:
	MulticastDelegate<> on_property_change_internal;

	void on_select_component(const ISelectionApi& api,Component* ec) {
		refresh_grid(api);
	}
	Component* get_selected_component(Entity* e) const;

	void draw_components(const ISelectionApi& api, Entity* entity);

	string component_filter;
	bool component_set_keyboard_focus = true;

	struct GridWithClasses {
	public:
		GridWithClasses(const FnFactory<IPropertyEditor>& factory) : grid(factory) {}
		bool set_what_i_want_and_draw(std::vector<obj<BaseUpdater>> objs);
		int row_count() const { return grid.rows.size(); }
	private:
		std::vector<obj<BaseUpdater>> cached_from_prev;
		PropertyGrid grid;
	};

	GridWithClasses grid_cache;
};
