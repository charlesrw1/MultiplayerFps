#pragma once
#include "Framework/ClassBase.h"
// MWHAAAA SCRIPTING THE EDITOR!!
// I AM HAVE GOD POWERS!!
#include "Game/Entity.h"
class EditorStatic : public ClassBase {
public:
	CLASS_BODY(EditorStatic);
	REF static std::vector<obj<BaseUpdater>> get_selection() {
		return {};
	}
};