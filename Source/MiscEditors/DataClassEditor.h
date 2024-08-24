#pragma once
#include "IEditorTool.h"

#include "DataClass.h"

#include "Framework/PropertyEd.h"

class DataClassEditor : public IEditorTool
{
	virtual bool open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	const ClassTypeInfo& get_asset_type_info() const override { return DataClass::StaticType; }
	void imgui_draw() override;

	PropertyGrid grid;
	std::string dc_name;
	ClassBase* editing_object = nullptr;
};