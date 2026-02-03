#pragma once
#include "Framework/Config.h"
#include <functional>
#include "Animation/Editor/Optional.h"
using std::function;
#include "Framework/ClassBase.h"
class SceneAsset;
enum class OpenMapReturnCode {
	Success,
	FailedToLoad,
	AlreadyLoadingMap
};

// renderer commands? I suppose these can be made as system commands since they are executed outside of overlap
#ifdef EDITOR_BUILD

// renders the skylight as an equirectnagular image and sends it to the callback
class ExportSkylightCommand : public SystemCommand {
public:
	ExportSkylightCommand() {}
	struct Data {
		int x = 0;
		int y = 0;
		float* data = nullptr;
	};
	function<void(Data)> callback;
	void execute() final;
	string to_string() final { return string("ExportSkylightCommand"); }
};
#endif