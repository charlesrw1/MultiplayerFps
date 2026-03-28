#pragma once
#include "IInputReciever.h"

class IEditorMode
{
public:
	virtual void tick() = 0;
};

class FoliagePaintTool : public IInputReciever, public IEditorMode
{
public:
	FoliagePaintTool(EditorDoc& doc) : doc(doc), ran(17) {}
	~FoliagePaintTool() {
		for (auto item : foliage)
			idraw->get_scene()->remove_obj(item.object);
	}
	void tick();

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
	void tick();

private:
	float rotation = 0.0;
	float scale = 1.0;
	float depth = 1.0;
	EntityPtr preview;
	EditorDoc& doc;
};
// the default tool
class SelectionMode : public IEditorMode
{
public:
	SelectionMode(EditorDoc& doc) : doc(doc) {}
	void tick();
	EditorDoc& doc;
};

