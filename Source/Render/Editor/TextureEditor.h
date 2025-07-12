#pragma once

#include "IEditorTool.h"
#include "Render/Texture.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/PropertyEd.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"

#include "LevelEditor/PropertyEditors.h"
#include "Framework/FnFactory.h"

extern bool compile_texture_asset(const std::string& gamepath,IAssetLoadingInterface*,Color32&);


class TextureImportSettings : public ClassBase {
public:
	CLASS_BODY(TextureImportSettings);

	REF bool is_generated = false;	// was this not sourced from a file, if so skip it
	REF std::string src_file;	// relative filepath, must be in same directory
	REF bool is_normalmap = false;
	REF bool is_srgb = false;

	REF Color32 simplifiedColor = COLOR_BLACK;
};
