#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Types.h"
#include "Render/DrawPublic.h"

// a base class for editor tools that want a simple scene with a camera, skybox, plane, and directional light
// used by animgraph, material, model, animseq, and sound editors
// The level editor DOENST inherit from this, it does its own thing

class StaticMeshEntity;
class EditorTool3d : public IEditorTool
{
public:
	const View_Setup* get_vs() override final {
		return &view;
	}
	void tick(float dt) override;

	// override this to load your assets etc.
	// use get_doc_name to get the name
	virtual void post_map_load_callback() = 0;

	bool open_document_internal(const char* name, const char* arg) override final;

	// call up to this in your close_internal! EditorTool3d::close_internal();
	void close_internal() override;
private:
	void map_callback(bool good);

	User_Camera camera;
	View_Setup view;
};
#endif