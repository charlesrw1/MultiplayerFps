#pragma once

#include "IEditorTool.h"
#include "Render/Texture.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/PropertyEd.h"
#include "Framework/Files.h"
#include "Framework/EnumDefReflection.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"

#include "LevelEditor/PropertyEditors.h"
#include "Framework/FnFactory.h"

extern bool compile_texture_asset(const std::string& gamepath, Color32&);

// Unset is only used as the on-disk-default sentinel for migrating pre-existing
// .tis files (see migrate_legacy_tis_compression); never write it out deliberately.
NEWENUM(TextureCompressionType, uint8_t){
	Unset,
	Compressed_BC1,		// general color textures, no alpha needed
	Uncompressed,		// R8G8B8A8, no compression artifacts
	NormalMap_BC5,		// tangent-space normal maps (XY channels)
	GreyscaleMask_BC4,	// single-channel masks (roughness, AO, etc)
	HighQuality_BC7,	// color + alpha, higher quality than BC1/BC3
	UseSourceFile,		// don't compress; load the source .png/.jpg directly at runtime (UI textures)
};

class TextureImportSettings : public ClassBase
{
public:
	CLASS_BODY(TextureImportSettings);

	REF bool is_generated = false; // was this not sourced from a file, if so skip it
	REF std::string src_file;	   // relative filepath, must be in same directory

	REF TextureCompressionType compression = TextureCompressionType::Unset;
	REF bool is_srgb = false; // applies to Compressed_BC1 and HighQuality_BC7 only
	REF int resize_width = 0; // ie 1024, 2048 etc resize source image

	// Deprecated, kept only so old .tis files still parse; migrated into
	// `compression` by migrate_legacy_tis_compression() and then ignored.
	REF bool is_normalmap = false;
	REF bool make_uncompressed = false;
	REF bool load_source_file = false;

	REF bool nearest_filtering = false;

	REF Color32 simplifiedColor = COLOR_BLACK;
};

extern void write_texture_import_settings(TextureImportSettings* tis, const std::string& path);
// Old .tis files only have is_normalmap/make_uncompressed/load_source_file bools; new ones have
// `compression` set directly. Called after every load: if compression is still Unset, derive it
// from the legacy bools once. No-op for files already on the new format.
extern void migrate_legacy_tis_compression(TextureImportSettings* tis);
