#include "UILoader.h"
#include <cassert>
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"

#include "Framework/BinaryReadWrite.h"
#include "imgui.h"

CLASS_IMPL(GuiFont);

// global
GuiFontLoader g_fonts;

#ifdef EDITOR_BUILD
class FontAssetMetadata : public AssetMetadata
{
public:
	FontAssetMetadata() {
		extensions.push_back("fnt");
	}
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 252, 245, 35 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Font";
	}


	virtual const ClassTypeInfo* get_asset_class_type() const { return &GuiFont::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(FontAssetMetadata);
#endif

#include "Render/Texture.h"
#define MAKE_FOUR(a,b,c,d) ( (uint32_t)a | ((uint32_t)b<< 8) | ((uint32_t)c << 16) | ((uint32_t)d<<24) )

void GuiFont::sweep_references() const {
	GetAssets().touch_asset((IAsset*)font_texture);
}

bool GuiFont::load_asset(ClassBase*& user)
{
	auto& path = get_name();
	auto file = FileSys::open_read_game(path.c_str());
	if (!file)
	{
		sys_print(Error, "couldn't open font: %s\n", path.c_str());
		return false;
	}
	BinaryReader in(file.get());
	auto magic_and_id = in.read_int32();
	if (magic_and_id != MAKE_FOUR('B', 'M', 'F', 3)) {
		sys_print(Error, "bad magic in font file\n");
		return false;
	}
	uint8_t blockid = in.read_byte();
	uint32_t blockSz = in.read_int32();
	ASSERT(blockid == 1);
	in.seek(in.tell() + blockSz);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 2);
	in.seek(in.tell() + blockSz);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 3);
	in.seek(in.tell() + blockSz);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 4);
	uint32_t numChars = blockSz / 20;
	for (uint32_t charIdx = 0; charIdx < numChars; charIdx++) {

		GuiFontGlyph glyph;

		auto id = in.read_int32();
		glyph.x = in.read_int16();
		glyph.y = in.read_int16();
		glyph.w = in.read_int16();
		glyph.h = in.read_int16();
		glyph.xofs = in.read_int16();
		glyph.yofs = in.read_int16();
		glyph.advance = in.read_int16();
		auto page = in.read_byte();
		auto chnl = in.read_byte();

		character_to_glyph.insert({ id,glyph });
	}
	font_texture = GetAssets().find_assetptr_unsafe<Texture>("eng/sengo24_0.png");
	return true;
}


#include "GameEnginePublic.h"
DECLARE_ENGINE_CMD(FONT_TEST)
{
	//auto font = g_fonts.load_font("courier_20.fnt");

	//auto sz = GuiHelpers::calc_text_size("This is a string", font);
}
void GuiFontLoader::init()
{
	defaultFont = GetAssets().find_global_sync<GuiFont>("eng/sengo24.fnt");
	if (!defaultFont)
		Fatalf("couldnt load default font");
}