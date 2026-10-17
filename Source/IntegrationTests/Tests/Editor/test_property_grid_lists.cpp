// Source/IntegrationTests/Tests/Editor/test_property_grid_lists.cpp
// Property grid rows for list elements: each editor holds a path ("eMaterialOverride[1]"), not a pointer, so a
// resize of the list does not leave it pointing at freed memory.
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#ifdef EDITOR_BUILD
#include "LevelEditor/PropertyEditors.h"
#include "Framework/FnFactory.h"
#include "Framework/PropertyEd.h"

namespace {
// The array row whose element editors have paths that start with name.
ArrayRow* find_array_row(PropertyGrid& grid, const std::string& name) {
	for (auto& group : grid.rows)
		for (auto& row : group->child_rows)
			if (auto* array = dynamic_cast<ArrayRow*>(row.get()); array && !array->child_rows.empty())
				if (auto* first = dynamic_cast<PropertyRow*>(array->child_rows[0].get()))
					if (first->prop_editor->path.to_string().rfind(name + "[", 0) == 0)
						return array;
	return nullptr;
}
IPropertyEditor* element_editor(ArrayRow* array, int index) {
	auto* row = dynamic_cast<PropertyRow*>(array->child_rows.at(index).get());
	return row ? row->prop_editor.get() : nullptr;
}
} // namespace

static TestTask test_property_grid_list_paths(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	t.require(doc != nullptr, "got EditorDoc");

	Entity* e = doc->spawn_entity();
	MeshComponent* mc = e->create_component<MeshComponent>();
	const PropertyInfo* overrides = MeshComponent::StaticType.props->find("eMaterialOverride");
	t.require(overrides != nullptr, "MeshComponent has eMaterialOverride");
	const PropertyRef list{mc, PropertyPath(overrides)};
	list.list_resize(2);
	{
		FnFactory<IPropertyEditor> factory;
		PropertyFactoryUtil::register_basic(factory);
		PropertyFactoryUtil::register_editor(*doc, factory);
		PropertyGrid grid(factory);
		grid.add_class_to_grid(mc);

		ArrayRow* array = find_array_row(grid, "eMaterialOverride");
		t.require(array != nullptr, "the grid has an array row for eMaterialOverride");
		t.check(array->child_rows.size() == 2, "one row per element");
		IPropertyEditor* second = element_editor(array, 1);
		t.require(second != nullptr, "element rows have editors");
		t.check(second->path.to_string() == "eMaterialOverride[1]", "the element editor holds a path");

		auto* asset_editor = dynamic_cast<AssetPropertyEditor*>(second);
		t.require(asset_editor != nullptr, "an AssetPtr element gets the asset editor");
		asset_editor->set_asset("basicGolden.mi");
		t.check(PropertyRef{mc, list.path.element(1)}.get<IAsset*>() != nullptr, "the editor wrote the element");
		t.check(asset_editor->get_str() == "basicGolden.mi", "the editor reads the element through its path");

		// The list grows past its capacity: the editor reaches the moved element through its path.
		list.list_resize(64);
		t.check(asset_editor->get_str() == "basicGolden.mi", "the path resolves after the list reallocates");
		list.list_resize(1);
		t.check(!second->target().resolves(), "the path of a removed element does not resolve");
	}
	e->destroy();
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/property_grid_list_paths", 20.f, test_property_grid_list_paths);
#endif
