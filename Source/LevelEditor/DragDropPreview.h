#pragma once

#include "AllHeader.h"

class DragDropPreview
{
public:
	void set_preview_model(Model* m, const glm::mat4& where);
	void set_preview_component(const ClassTypeInfo* t, const glm::mat4& where);
	void tick();

private:
	void fixup_entity();
	void delete_obj();
	bool had_state_set = false;
	enum class State
	{
		None,
		PreviewModel,
		PreviewPrefab,
		PreviewComponent
	} state = State::None;
	Model* preview_model = nullptr;
	const ClassTypeInfo* preview_comp = nullptr;
	obj<Entity> obj_ptr;
};