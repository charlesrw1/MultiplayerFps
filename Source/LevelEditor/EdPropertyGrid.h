#pragma once
#include "AllHeader.h"
#include "Framework/PropertyEd.h"
class EdPropertyGrid
{
public:
	EdPropertyGrid(EditorDoc& ed_doc, const FnFactory<IPropertyEditor>& factory);
	void draw();
	MulticastDelegate<> on_property_change;

private:
	void on_ec_deleted(uint64_t comp) {
		if (selected_component == comp)
			selected_component = 0;
		refresh_grid();
	}
	void on_close() { grid.clear_all(); }
	void refresh_grid();

	uint64_t selected_component = 0;
	uint64_t component_context_menu = 0;

	void on_select_component(Component* ec) {
		selected_component = ec->get_instance_id();
		refresh_grid();
	}

	Component* get_selected_component() const {
		if (selected_component == 0)
			return nullptr;
		auto o = eng->get_object(selected_component);
		if (!o)
			return nullptr;
		return o->cast_to<Component>();
	}

	void draw_components(Entity* entity);

	EditorDoc& ed_doc;
	PropertyGrid grid;
	const FnFactory<IPropertyEditor>& factory;

	string component_filter;
	bool component_set_keyboard_focus = true;
};
