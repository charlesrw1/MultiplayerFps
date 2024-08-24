#include "DataClassEditor.h"

#include "AssetCompile/Someutils.h"
#include "GameEnginePublic.h"
#include "Framework/ObjectSerialization.h"
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"
#include "Render/DrawPublic.h"	// for dummy View_Setup


static DataClassEditor g_dced_local;
IEditorTool* g_dataclass_editor=&g_dced_local;



bool DataClassEditor::open_document_internal(const char* name, const char* arg)
{
	assert(editing_object == nullptr);

	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini DataClassEditor.ini");

	// loading a file
	if (has_extension(name, "dc")) {
		const DataClass* dc = GetAssets().find_sync<DataClass>(name).get();
		if (dc) {
			editing_object = dc->get_obj()->get_type().allocate();
			copy_object_properties((ClassBase*)dc->get_obj(), editing_object, nullptr);
		}
	}
	else {
		// loading an exisitng class object
		
		auto type =  ClassBase::find_class(name);
		if (type&&type->allocate) {
			editing_object = type->allocate();
		}
	}
	if (!editing_object) {
		sys_print("!!! DataClassEditor couldnt find class to edit with name: %s\n", name);
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "close_ed");
		// this should call close_internal etc.
		return false;
	}

	auto ti = &editing_object->get_type();
	while (ti) {
		if (ti->props)
			grid.add_property_list_to_grid(ti->props, editing_object);
		ti = ti->super_typeinfo;
	}
}

void DataClassEditor::close_internal()
{
	if (!editing_object)
		return;

	grid.clear_all();

	delete editing_object;
	editing_object = nullptr;

}
#include <fstream>
bool DataClassEditor::save_document_internal()
{

	assert(editing_object);

	std::string savepath = "./Data/";
	savepath += get_doc_name();

	DictWriter dw;
	write_object_properties(editing_object, nullptr, dw);

	// save as text
	std::ofstream outfile(savepath);
	outfile.write(dw.get_output().data(), dw.get_output().size());
	outfile.close();

	return true;
}


void DataClassEditor::imgui_draw() {

	if (ImGui::Begin("DataClassEditor")) {
		grid.update();
	}
	ImGui::End();

	IEditorTool::imgui_draw();
}