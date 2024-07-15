#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <memory>
#include "Framework/Util.h"
#include "Framework/ReflectionProp.h"

// Property editor system, took some of the code structure from https://github.com/BobbyAnguelov/Esoterica

typedef std::vector<const char*>* (*find_completion_strs_callback)(void* user, const char* str, int len);

struct ImguiInputTextCallbackUserStruct
{
	std::string* string = nullptr;

	find_completion_strs_callback fcsc = nullptr;
	void* fcsc_user_data = nullptr;
};

int imgui_input_text_callback_function(ImGuiInputTextCallbackData* data);

// draws an imgui tool to edit the property passed in
class IPropertyEditor
{
public:
	static Factory<std::string, IPropertyEditor>& get_factory();

	IPropertyEditor()  {}

	void post_construct_for_custom_type(void* instance, PropertyInfo* prop) {
		this->instance = instance;
		this->prop = prop;
	}

	void update();
	virtual void internal_update() = 0;
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return false; }
	virtual void reset_value() {}

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
};

// optional: gets the string to use for an array header for a given index and what to draw when the item is closed
class IArrayHeader
{
public:
	static Factory<std::string, IArrayHeader>& get_factory();


	IArrayHeader() {
	
	}
	void post_construct(void* instance, PropertyInfo* prop) {
		ASSERT(prop->type == core_type_id::List && prop->list_ptr);
		this->instance = instance;
		this->prop = prop;
	}

	virtual bool imgui_draw_header(int index) = 0;
	virtual void imgui_draw_closed_body(int index) = 0;
	virtual bool has_delete_all() { return true; }
	virtual bool can_edit_array() { return true; }
	
	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
};

class IGridRow
{
public:
	IGridRow(IGridRow* parent, int row_index = -1) : parent(parent), row_index(row_index) {}
	virtual ~IGridRow() {}
	virtual void update(float header_ofs);
	virtual void internal_update() = 0;
	virtual void draw_header(float header_ofs) = 0;

	virtual bool draw_children() {
		return true;
	}
	virtual float get_indent_width() { return 18.0; }

	void clear_children();

	virtual bool has_reset_button() { return false; }
	virtual void on_reset() {}
	virtual bool has_row_controls() { return false; }
	virtual void draw_row_controls() {}
	virtual bool passthrough_to_child() { return false; }
	virtual bool is_array() { return false; }

	void set_name_override(const std::string& str) { name_override = str; }

	std::string name_override = "";
	bool expanded = true;	// for array and group rows
	int row_index = -1;	// if > 0, then row is part of an array
	IGridRow* parent = nullptr;
	std::vector<std::unique_ptr<IGridRow>> child_rows;
};


enum PropertyGridFlags
{
	PG_LIST_PASSTHROUGH = 1
};

class PropertyGrid
{
public:
	void clear_all() {
		rows.clear();
	}

	void add_property_list_to_grid(
		const PropertyInfoList* list, 
		void* inst, 
		uint32_t flags = 0 /* PropertyGridFlags */, 
		uint32_t property_flag_mask = UINT32_MAX /* specifiy a mask that gets ANDd with each properties flags, will skip if its 0 */
	);
	void update();

	void set_read_only(bool read_only) {
		this->read_only = read_only;
	}

	bool read_only = false;
	std::vector<std::unique_ptr<IGridRow>> rows;
};


class GroupRow : public IGridRow
{
public:
	GroupRow(IGridRow* parent, void* instance, const PropertyInfoList* info, int row_idx, uint32_t property_flag_mask);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool draw_children() override;

	virtual float get_indent_width() { return 30.0; }

	virtual void draw_row_controls() override;
	virtual bool has_row_controls() { return row_index != -1; }

	virtual bool passthrough_to_child() override {
		return parent && parent->is_array() && proplist->count == 1;
	}

	void* inst = nullptr;
	const PropertyInfoList* proplist = nullptr;
	std::string name;
	bool passthrough_to_list_if_possible = false;
};

class ArrayRow : public IGridRow
{
public:
	ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx, uint32_t property_flag_mask);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool has_row_controls() override { return true; }
	virtual void draw_row_controls();
	virtual float get_indent_width() { return 30.0; }
	virtual bool is_array() override { return true; }
	void rebuild_child_rows();
	void hook_update_pre_tree_node();
	int get_size();

	bool are_any_nodes_open();

	void delete_index(int index) {
		commands.push_back({ Delete, index });
	}
	void moveup_index(int index) {
		commands.push_back({ Moveup, index });
	}
	void movedown_index(int index) {
		commands.push_back({ Movedown, index });
	}

	std::unique_ptr<IArrayHeader> header = nullptr;	/* can be nullptr */
private:

	enum CommandEnum {
		Moveup,
		Movedown,
		Delete,
	};

	enum class next_state {
		keep,
		hidden,
		visible
	}set_next_state = next_state::keep;

	struct CommandData {
		CommandEnum command;
		int index = 0;
	};

	std::vector<CommandData> commands;

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;

	uint32_t property_flag_mask = UINT32_MAX;
};
class PropertyRow : public IGridRow
{
public:
	PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool has_reset_button() override { return prop_editor->can_reset(); }
	virtual void on_reset() override { prop_editor->reset_value(); }

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;

	std::unique_ptr<IPropertyEditor> prop_editor = nullptr;
};

class StringEditor : public IPropertyEditor
{
public:
	StringEditor(void* inst, PropertyInfo* prop) {
		this->instance = inst;
		this->prop = prop;
	}
	// Inherited via IPropertyEditor
	virtual void internal_update() override;

	virtual bool can_reset() override;
	virtual void reset_value() override;
};

class FloatEditor : public IPropertyEditor
{
public:
	FloatEditor(void* inst, PropertyInfo* prop) { 
		this->instance = inst;
		this->prop = prop;
		hint_str = parse_hint_str_for_property(prop); 
	}

	// Inherited via IPropertyEditor
	virtual void internal_update() override;

	virtual bool can_reset() override {
		return hint_str.has_default&& (abs(prop->get_float(instance) - hint_str.default_f) > 0.000001);
	}
	virtual void reset_value() override {
		prop->set_float(instance, hint_str.default_f);
	}

	ParsedHintStr hint_str;
};

class IntegerEditor : public IPropertyEditor
{
public:
	IntegerEditor(void* inst, PropertyInfo* prop)  { 
		this->instance = inst;
		this->prop = prop;
		hint_str = parse_hint_str_for_property(prop);
	}


	// Inherited via IPropertyEditor
	virtual void internal_update() override;
	virtual bool can_reset() override {
		return hint_str.has_default&& prop->get_int(instance) != hint_str.default_i;
	}
	virtual void reset_value() override {
		prop->set_int(instance, hint_str.default_i);
	}
	ParsedHintStr hint_str;
};

class EnumEditor : public IPropertyEditor
{
public:
	EnumEditor(void* inst, PropertyInfo* prop) { 
		this->instance = inst;
		this->prop = prop;
		hint_str = parse_hint_str_for_property(prop); 
	}

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
	virtual bool can_reset() override {
		return hint_str.has_default&& prop->get_int(instance) != hint_str.default_i;
	}
	virtual void reset_value() override {
		prop->set_int(instance, hint_str.default_i);
	}
	ParsedHintStr hint_str;
};

class BooleanEditor : public IPropertyEditor
{
public:
	BooleanEditor(void* inst, PropertyInfo* prop) { 
		this->instance = inst;
		this->prop = prop;
		hint_str = parse_hint_str_for_property(prop);
	}

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
	virtual bool can_reset() override {
		return hint_str.has_default&& prop->get_int(instance) != hint_str.default_i;
	}
	virtual void reset_value() override {
		prop->set_int(instance, hint_str.default_i);
	}
	ParsedHintStr hint_str;
};
