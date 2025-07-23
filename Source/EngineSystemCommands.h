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
class OpenMapCommand : public SystemCommand
{
public:

	OpenMapCommand(opt<string> map_name, bool is_for_playing) : map_name(map_name), is_for_playing(is_for_playing) {}
	void execute() final;
	string to_string() final { return "OpenMapCommand: " + map_name.value_or("<empty>"); }
	function<void(OpenMapReturnCode)> callback;
private:
	opt<string> map_name;
	bool is_for_playing = false;
};

// renderer commands? I suppose these can be made as system commands since they are executed outside of overlap
#ifdef EDITOR_BUILD
class OpenEditorToolCommand : public SystemCommand {
public:
	OpenEditorToolCommand(const ClassTypeInfo& assetType, opt<string> assetName/* pass null for a new asset*/, bool set_active)
		: assetType(assetType), assetName(assetName), set_active(set_active) {}
	void execute() final;
	string to_string() final { return string("OpenEditorToolCommand: ") + assetType.classname; }
	function<void(bool)> callback;
private:
	bool set_active = false;
	const ClassTypeInfo& assetType;
	opt<string> assetName;
};
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