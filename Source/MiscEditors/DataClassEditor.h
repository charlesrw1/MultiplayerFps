#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "DataClass.h"
#include "Framework/PropertyEd.h"
#include "Framework/FnFactory.h"

class DataClassEditor : public IEditorTool
{
public:
	DataClassEditor();
	virtual bool open_document_internal(const char* name, const char* arg);
	virtual void close_internal();

	virtual bool save_document_internal() override;
	const ClassTypeInfo& get_asset_type_info() const override { return DataClass::StaticType; }
	void imgui_draw() override;
	const char* get_save_file_extension() const {
		return "dc";
	}

	void refresh();

	FnFactory<IPropertyEditor> factory;
	PropertyGrid grid;
	std::string dc_name;
	ClassBase* editing_object = nullptr;
	const ClassTypeInfo* typeInfo = nullptr;
};
#endif