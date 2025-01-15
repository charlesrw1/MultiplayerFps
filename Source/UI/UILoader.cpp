#include "UILoader.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"

#include "Framework/BinaryReadWrite.h"
#include "imgui.h"
CLASS_IMPL(GUI);
CLASS_IMPL(GuiFont);

// global
GuiFontLoader g_fonts;

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
#include "Render/Texture.h"
REGISTER_ASSETMETADATA_MACRO(FontAssetMetadata);
#define MAKE_FOUR(a,b,c,d) ( (uint32_t)a | ((uint32_t)b<< 8) | ((uint32_t)c << 16) | ((uint32_t)d<<24) )


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
	font_texture = GetAssets().find_assetptr_unsafe<Texture>("sengo24_0.png");
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
	defaultFont = GetAssets().find_global_sync<GuiFont>("sengo24.fnt");
	if (!defaultFont)
		Fatalf("couldnt load default font");
}
glm::ivec2 GuiHelpers::calc_text_size_no_wrap(const char* str, const GuiFont* font)
{
	int x = 0;
	while (*str) {
		char c = *str;
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10;	// empty character
		}
		else
			x += find->second.advance;

		str++;
	}
	return { x, 24 };
}
glm::ivec2 GuiHelpers::calc_text_size(const char* str, const GuiFont* font, int force_width )
{
	ASSERT(font);

	if (force_width == -1)
		return calc_text_size_no_wrap(str, font);

	std::string currentLine;
	std::string currentWord;

	int x = 0;
	int y = 0;
	while (*(str++)) {
		char c = *str;
		if (c == ' ' || c == '\n') {
			auto sz = calc_text_size_no_wrap((currentLine + currentWord).c_str(), font);
			if (sz.x > force_width)
			{
				x = glm::max(sz.x, x);
				y += font->ptSz;
				currentLine = currentWord + " ";
			}
			else
				currentLine += currentWord + " ";
			currentWord.clear();
			if (c == '\n') {
				y += font->ptSz;
				currentLine.clear();
			}
		}
		else
			currentWord += c;
	}
	if (!currentWord.empty()) {
		currentLine += currentWord;

		x = glm::max(calc_text_size_no_wrap(currentLine.c_str(), font).x, x);
		y += font->ptSz;
	}

	return { x, y };
}