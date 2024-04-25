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
	IGridRow(IGridRow* parent) : parent(parent) {}

	virtual void update();
	virtual void internal_update() = 0;
	virtual void draw_header() = 0;

	IGridRow* parent = nullptr;
	std::vector<std::unique_ptr<IGridRow>> child_rows;
};

class ArrayRow : public IGridRow
{
public:
	ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop);

	virtual void internal_update() override;
	virtual void draw_header() override;

	void* instance = nullptr;
	PropertyInfo* prop = nullptr;
};
class PropertyRow : public IGridRow
{
public:
	PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop);

	virtual void internal_update() override;
	virtual void draw_header() override;

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
