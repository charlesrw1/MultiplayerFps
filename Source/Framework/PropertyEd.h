#pragma once
#ifdef EDITOR_BUILD
#include "imgui.h"
#include <string>
#include <vector>
#include <memory>
#include "Framework/Util.h"
#include "Framework/ReflectionProp.h"
#include "Framework/PropertyPath.h"
#include "Framework/StringName.h"

// Property editor system, took some of the code structure from https://github.com/BobbyAnguelov/Esoterica

typedef std::vector<const char*>* (*find_completion_strs_callback)(void* user, const char* str, int len);

struct ImguiInputTextCallbackUserStruct
{
	std::string* string = nullptr;

	find_completion_strs_callback fcsc = nullptr;
	void* fcsc_user_data = nullptr;
};

int imgui_input_text_callback_function(ImGuiInputTextCallbackData* data);

// "Float", "vector<Int32>" and similar, for tooltips.
std::string property_type_name(const PropertyInfo* p);
// The type and tooltip of the last item, when it is hovered.
void draw_property_tooltip(const PropertyInfo* prop);

// draws an imgui tool to edit the property passed in
class IGridRow;
class IPropertyEditor
{
public:
	IPropertyEditor() {}
	virtual ~IPropertyEditor() {}

	void post_construct(const PropertyRef& target, IGridRow* parent) {
		ASSERT(target.root && !target.path.empty());
		instance = target.root;
		path = target.path;
		prop = const_cast<PropertyInfo*>(&target.info());
	}
	// A property of root itself (no struct or list on the way).
	void post_construct_for_custom_type(void* root, PropertyInfo* p, IGridRow* parent) {
		post_construct(PropertyRef{root, PropertyPath(p)}, parent);
	}

	bool update();

	// return true if the property was updated
	virtual bool internal_update() = 0;
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return false; }
	virtual void reset_value() {}

	// The edited value, through the path.
	PropertyRef target() const { return PropertyRef{instance, path}; }
	template <typename T> T get() const { return target().get<T>(); }
	template <typename T> void set(const T& value) const { target().set(value); }

	void* instance = nullptr;	 // the owner object at the start of path
	PropertyInfo* prop = nullptr; // describes the edited value
	PropertyPath path;
};

class PropertyGrid;
class IGridRow
{
public:
	IGridRow(IGridRow* parent, int row_index = -1) : parent(parent), row_index(row_index) {}
	virtual ~IGridRow() {}
	virtual void update(PropertyGrid* parentGrid, float header_ofs);
	virtual bool internal_update() = 0;
	virtual void draw_header(float header_ofs) = 0;

	virtual bool draw_children() { return true; }
	virtual float get_indent_width() { return 18.f; }

	void clear_children();

	virtual bool has_reset_button() { return false; }
	virtual void on_reset() {}
	virtual bool has_row_controls() { return false; }
	virtual bool draw_row_controls() { return false; }
	virtual bool passthrough_to_child() { return false; }
	virtual bool is_array() { return false; }

	void set_name_override(const std::string& str) { name_override = str; }

	std::string name_override = "";
	bool expanded = true; // for array and group rows
	int row_index = -1;	  // if > 0, then row is part of an array
	IGridRow* parent = nullptr;
	std::vector<std::unique_ptr<IGridRow>> child_rows;
};

template <typename T> class FnFactory;
// The editor of a value property: the factory editor named by custom_type_str, else the one for its type.
std::unique_ptr<IPropertyEditor> create_ipropertyed(const FnFactory<IPropertyEditor>& factory, const PropertyRef& target,
													IGridRow* parent);

enum PropertyGridFlags
{ PG_LIST_PASSTHROUGH = 1 };
class ClassBase;
class PropertyGrid
{
public:
	PropertyGrid(const FnFactory<IPropertyEditor>& factory);

	void clear_all() { rows.clear(); }

	void add_property_list_to_grid(const PropertyInfoList* list, void* inst, uint32_t flags = 0 /* PropertyGridFlags */,
								   uint32_t property_flag_mask = UINT32_MAX /* specifiy a mask that gets ANDd with each
																			   properties flags, will skip if its 0 */
	);

	void add_iproped_manual(IPropertyEditor* editor);

	void add_class_to_grid(ClassBase* classinst);

	void update();

	void set_read_only(bool read_only) { this->read_only = read_only; }

	void set_rows_had_changes() { rows_had_changes = true; }

	bool rows_had_changes = false;
	bool read_only = false;
	std::vector<std::unique_ptr<IGridRow>> rows;
	const FnFactory<IPropertyEditor>& factory;
};

// The properties of an object (base is empty) or of the struct that base reaches.
class GroupRow : public IGridRow
{
public:
	GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* root, const PropertyInfoList* list,
			 int row_idx, uint32_t property_flag_mask);
	GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& base, int row_idx,
			 uint32_t property_flag_mask);

	virtual bool internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool draw_children() override;

	virtual float get_indent_width() { return 12.0; }

	virtual bool has_row_controls() override { return false; }

	virtual bool passthrough_to_child() override { return parent && parent->is_array() && proplist->count == 1; }

	const PropertyInfo* property = nullptr; // the struct property, null for an object
	const PropertyInfoList* proplist = nullptr;
	std::string name;

private:
	void add_rows(const FnFactory<IPropertyEditor>& factory, const PropertyRef& base, uint32_t property_flag_mask);
};

class ArrayRow : public IGridRow
{
public:
	ArrayRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& list, int row_idx,
			 uint32_t property_flag_mask);

	virtual bool internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool has_row_controls() override { return false; }
	virtual float get_indent_width() { return 12.0; }
	virtual bool is_array() override { return true; }
	void rebuild_child_rows();
	void hook_update_pre_tree_node();
	int get_size();

	bool are_any_nodes_open();

	void delete_index(int index) { commands.push_back({Delete, index}); }
	void reorder_index(int from, int to) { commands.push_back({Reorder, from, to}); }

	bool pending_add = false;
	bool pending_clear = false;
private:
	enum CommandEnum
	{
		Reorder,
		Delete,
	};

	enum class next_state
	{
		keep,
		hidden,
		visible
	} set_next_state = next_state::keep;

	struct CommandData
	{
		CommandEnum command;
		int index = 0;
		int index2 = 0; // used by Reorder (target index)
	};

	const FnFactory<IPropertyEditor>& factory;
	std::vector<CommandData> commands;
	PropertyRef list;
	const PropertyInfo* prop = nullptr;
	uint32_t property_flag_mask = UINT32_MAX;
};
class PropertyRow : public IGridRow
{
public:
	PropertyRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& target, int row_idx);
	PropertyRow(IPropertyEditor* editor, IGridRow* parent, int row_idx);

	bool internal_update() override;
	void draw_header(float header_ofs) override;
	bool has_reset_button() override { return prop_editor->can_reset(); }
	void on_reset() override { prop_editor->reset_value(); }

	const PropertyInfo* prop = nullptr;

	std::unique_ptr<IPropertyEditor> prop_editor = nullptr;
};

#endif