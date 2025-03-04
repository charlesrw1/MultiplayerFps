#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Render/Texture.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/PropertyEd.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Framework/ObjectSerialization.h"

extern bool compile_texture_asset(const std::string& gamepath);

CLASS_H(TextureImportSettings,ClassBase)
public:
	bool is_generated = false;	// was this not sourced from a file, if so skip it
	std::string src_file;	// relative filepath, must be in same directory
	bool is_normalmap = false;
	bool is_srgb = false;
	static const PropertyInfoList* get_props() {
		START_PROPS(TextureImportSettings)
			REG_STDSTRING(src_file,PROP_DEFAULT),
			REG_BOOL(is_normalmap, PROP_DEFAULT,"0"),
			REG_BOOL(is_srgb, PROP_DEFAULT, "0"),
			REG_BOOL(is_generated, PROP_SERIALIZE, "0")
		END_PROPS(TextureImportSettings)
	}
};

class TextureEditorTool : public IEditorTool
{
public:
	// Inherited via IEditorTool
	virtual const ClassTypeInfo& get_asset_type_info() const override;
	virtual bool open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	const char* get_save_file_extension() const {
		return "dds";
	}
	void imgui_draw() override {
		if (ImGui::Begin("TextureEditorTool")) {
			grid.update();
		}
		ImGui::End();
		if (ImGui::Begin("TextureViewier")) {
			if (textureAsset)
				ImGui::Image((ImTextureID)uint64_t(textureAsset->gl_id), { 512,512 });
			else
				ImGui::Text("No texture asset to view.\n");
		}
		ImGui::End();
	}

	PropertyGrid grid;
	TextureImportSettings* editing_object = nullptr;
	Texture* textureAsset = nullptr;
};

inline const ClassTypeInfo& TextureEditorTool::get_asset_type_info() const
{
	return Texture::StaticType;
}

inline bool TextureEditorTool::open_document_internal(const char* name, const char* arg)
{
	// input: name.tex
	auto path = strip_extension(name) + ".tis";

	auto file = FileSys::open_read_game(path.c_str());
	if (!file) {
		sys_print(Error, "couldn't open texture import settings file %s\n", name);
		return false;
	}
	textureAsset = g_assets.find_sync<Texture>(name).get();


	DictParser dp;
	dp.load_from_file(file.get());
	auto classLoaded = read_object_properties_no_input_tok<TextureImportSettings>(nullptr, dp);
	if (!classLoaded) {
		sys_print(Error, "couldnt parse texture import settings %s\n", name);
		return false;
	}
	editing_object = classLoaded;

	grid.add_class_to_grid(editing_object);

	return true;
}

inline void TextureEditorTool::close_internal()
{
	if (!editing_object)
		return;
	grid.clear_all();
	delete editing_object;
	editing_object = nullptr;
}

inline bool TextureEditorTool::save_document_internal()
{
	auto path = strip_extension(get_doc_name()) + ".tis";
	auto file = FileSys::open_write_game(path);

	DictWriter out;
	write_object_properties(editing_object, nullptr, out);
	file->write(out.get_output().data(), out.get_output().size());
	file->close();

	if (textureAsset)
		g_assets.reload_sync(textureAsset);
	else
		textureAsset = g_assets.find_sync<Texture>(get_doc_name()).get();

	return true;
}
#endif