#ifdef EDITOR_BUILD
#include "DataClassEditor.h"

#include "AssetCompile/Someutils.h"
#include "GameEnginePublic.h"

#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"
#include "Render/DrawPublic.h"	// for dummy View_Setup
#include "LevelEditor/PropertyEditors.h"

//static DataClassEditor g_dced_local;
//IEditorTool* g_dataclass_editor=&g_dced_local;


DataClassEditor::DataClassEditor() : grid(factory)
{
	PropertyFactoryUtil::register_basic(factory);
}

bool DataClassEditor::open_document_internal(const char* name, const char* arg)
{
	assert(editing_object == nullptr);
	ASSERT(typeInfo == nullptr);

	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini DataClassEditor.ini");

	//eng->leave_level();

	// loading a file
	if (has_extension(name, "dc")) {
		const DataClass* dc = g_assets.find_sync<DataClass>(name).get();
		if (dc) {
			editing_object = ((ClassBase*)dc->get_obj())->create_copy();
			typeInfo = &editing_object->get_type();
		}
		else
			set_empty_doc();
	}
	else {
		// loading an exisitng class object

		set_empty_doc();
		
		typeInfo =  ClassBase::find_class(name);
		if (!typeInfo || !typeInfo->allocate) {
			sys_print(Error, "Couldnt find Class (or with allocator) for type: %s\n", name);
			typeInfo = nullptr;
		}
	}

	refresh();

	return true;
}
void DataClassEditor::refresh()
{
	grid.clear_all();

	if (!editing_object && typeInfo)
		editing_object = typeInfo->allocate();
	else if (editing_object && &editing_object->get_type() != typeInfo) {
		delete editing_object;
		editing_object = nullptr;
		if (typeInfo && typeInfo->allocate)
			editing_object = typeInfo->allocate();
	}

	if (editing_object) {
		auto ti = &editing_object->get_type();
		while (ti) {
			if (ti->props)
				grid.add_property_list_to_grid(ti->props, editing_object);
			ti = ti->super_typeinfo;
		}
	}
}

void DataClassEditor::close_internal()
{
	typeInfo = nullptr;
	if (!editing_object)
		return;

	grid.clear_all();

	delete editing_object;
	editing_object = nullptr;

}
#include "Framework/ReflectionProp.h"
#include "Framework/DictWriter.h"
#include <fstream>
bool DataClassEditor::save_document_internal()
{

	assert(editing_object);


	DictWriter dw;
	write_object_properties(editing_object, nullptr, dw);

	// save as text
	auto outfile = FileSys::open_write_game(get_doc_name());
	outfile->write(dw.get_output().data(), dw.get_output().size());
	outfile->close();

	return true;
}


void DataClassEditor::imgui_draw() {

	if (ImGui::Begin("DataClassEditor")) {

		bool has_update = false;
		const char* preview = (typeInfo) ? (typeInfo)->classname : "<empty>";
		if (ImGui::BeginCombo("##combocalsstype", preview)) {
			auto subclasses = ClassBase::get_subclasses<ClassBase>();
			for (; !subclasses.is_end(); subclasses.next()) {

				if (ImGui::Selectable(subclasses.get_type()->classname,
					subclasses.get_type() == typeInfo
				)) {
					typeInfo = subclasses.get_type();
					has_update = true;
				}

			}
			ImGui::EndCombo();
		}
		ImGui::Separator();
		if (ImGui::Button("Refresh")) {
			refresh();
		}
		ImGui::SameLine();
		if (ImGui::Button("Duplicate")) {
			if (typeInfo) {
				const char* str = string_format("start_ed DataClass %s", typeInfo->classname);
				Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, str);
			}
		}



		grid.update();
	}
	ImGui::End();
}
#endif