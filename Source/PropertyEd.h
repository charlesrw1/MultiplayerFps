#pragma once

#include "imgui.h"
#include <string>
#include <vector>
#include <memory>
#include "Util.h"
#include "ReflectionProp.h"

typedef std::vector<const char*>* (*find_completion_strs_callback)(void* user, const char* str, int len);

struct ImguiInputTextCallbackUserStruct
{
	std::string* string = nullptr;

	find_completion_strs_callback fcsc = nullptr;
	void* fcsc_user_data = nullptr;
};

int imgui_input_text_callback_function(ImGuiInputTextCallbackData* data);


class IPropertyEditor
{
public:
	IPropertyEditor(void* instance, PropertyInfo* prop) : instance(instance), prop(prop) {}

	void update();
	virtual void internal_update() = 0;
	virtual int extra_row_count() { return 0; }

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
};

class IPropertyEditorFactory
{
public:
	IPropertyEditorFactory();

	static IPropertyEditor* create(PropertyInfo* prop, void* instance);

	virtual IPropertyEditor* try_create(PropertyInfo* prop, void* instance) = 0;
private:
	IPropertyEditorFactory* next = nullptr;
	static IPropertyEditorFactory* first;
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

	int row_index = -1;	// if > 0, then row is part of an array
	IGridRow* parent = nullptr;
	std::vector<std::unique_ptr<IGridRow>> child_rows;
};

class GroupRow : public IGridRow
{
public:
	GroupRow(IGridRow* parent, void* instance, PropertyInfoList* info, int row_idx);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;
	virtual bool draw_children() override;

	virtual float get_indent_width() { return 30.0; }


	bool passthrough_to_child() {
		return row_index != -1 && child_rows.size() == 1;
	}

	void* inst = nullptr;
	PropertyInfoList* proplist = nullptr;
	std::string name;
};

class ArrayRow : public IGridRow
{
public:
	ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;

	virtual float get_indent_width() { return 30.0; }

	void rebuild_child_rows();

	int get_size();

	void delete_index(int index) {
		commands.push_back({ Delete, index });
	}
	void moveup_index(int index) {
		commands.push_back({ Moveup, index });
	}
	void movedown_index(int index) {
		commands.push_back({ Movedown, index });
	}
private:

	enum CommandEnum {
		Moveup,
		Movedown,
		Delete,
	};
	struct CommandData {
		CommandEnum command;
		int index = 0;
	};

	std::vector<CommandData> commands;

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
};
class PropertyRow : public IGridRow
{
public:
	PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx);

	virtual void internal_update() override;
	virtual void draw_header(float header_ofs) override;

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
	std::unique_ptr<IPropertyEditor> prop_editor = nullptr;
};

class PropertyGrid
{
public:
	void clear_all() {
		rows.clear();
	}

	void add_property_list_to_grid(PropertyInfoList* list, void* inst);
	void update();

	void set_read_only(bool read_only) {
		this->read_only = read_only;
	}

	bool read_only = false;
	std::vector<std::unique_ptr<IGridRow>> rows;
};

class StringEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
};

class FloatEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
};

class IntegerEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
};

class EnumEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
};

class BooleanEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override;
};
