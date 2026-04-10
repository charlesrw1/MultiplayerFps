#pragma once
#include "IInputReciever.h"
#include "AllHeader.h"
#include "DragDetector.h"
class IEditorMode
{
public:
	virtual void tick(EditorInputs& inputs) = 0;

	virtual void draw_ui()  {}
};

class FoliagePaintTool : public IInputReciever, public IEditorMode
{
public:
	FoliagePaintTool(EditorDoc& doc) : doc(doc), ran(17) {}
	~FoliagePaintTool() {
		for (auto item : foliage)
			idraw->get_scene()->remove_obj(item.object);
	}
	void tick(EditorInputs& inputs);

private:
	struct FoliageItem
	{
		handle<Render_Object> object;
		glm::vec3 pos;
	};
	std::vector<FoliageItem> foliage;
	EditorDoc& doc;
	Random ran;

	EntityPtr orb_cursor;
};

class DecalStampTool : public IInputReciever, public IEditorMode
{
public:
	DecalStampTool(EditorDoc& doc) : doc(doc) {}
	~DecalStampTool() {
		if (preview.get())
			preview->destroy();
	}
	void tick(EditorInputs& inputs);

private:
	float rotation = 0.0;
	float scale = 1.0;
	float depth = 1.0;
	EntityPtr preview;
	EditorDoc& doc;
};
// Road network editor (Cities-Skylines style)
// Full definition in RoadBuilderTool.h
class RoadBuilderTool;

// the default tool
class SelectionMode : public IEditorMode
{
public:
	SelectionMode(EditorDoc& doc);
	void tick(EditorInputs& inputs);

	void draw_ui() final;

	EditorDoc& doc;
	DragDetector dragger;
};

