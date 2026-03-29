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
	void on_ec_deleted(uint64_t comp) {
		if (selected_component == comp)
			selected_component = 0;
		if (last_api)
			refresh_grid(*last_api);
	}
	void on_close() { grid.clear_all(); }

	void on_select_component(Component* ec) {
		selected_component = ec->get_instance_id();
		if (last_api)
			refresh_grid(*last_api);
	}

	void refresh_grid(const ISelectionApi& api);

private:
	MulticastDelegate<> on_property_change_internal;
	const ISelectionApi* last_api = nullptr;

	Component* get_selected_component() const {
		if (selected_component == 0)
			return nullptr;
		auto o = eng->get_object(selected_component);
		if (!o)
			return nullptr;
		return o->cast_to<Component>();
	}

	void draw_components(const ISelectionApi& api, Entity* entity);

	uint64_t selected_component = 0;
	uint64_t component_context_menu = 0;
	PropertyGrid grid;
	const FnFactory<IPropertyEditor>& factory;
	string component_filter;
	bool component_set_keyboard_focus = true;
};
