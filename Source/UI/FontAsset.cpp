#include "FontAsset.h"
#include <cassert>
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Render/MaterialPublic.h"
#include "Framework/BinaryReadWrite.h"
#include "imgui.h"
#include "GameEnginePublic.h"

#ifdef EDITOR_BUILD
class FontAssetMetadata : public AssetMetadata
{
public:
	FontAssetMetadata() { extensions.push_back("fnt"); }
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return {252, 245, 35}; }

	virtual std::string get_type_name() const override { return "Font"; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &FontAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(FontAssetMetadata);
#endif

#include "Render/Texture.h"
#define MAKE_FOUR(a, b, c, d) ((uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24))

#include "Framework/StringUtils.h"

void FontAsset::post_load() {}
bool FontAsset::load_asset() {

	auto& path = get_name();
	auto file = FileSys::open_read_game(path.c_str());
	if (!file) {
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
	uintptr_t block2start = in.tell() + blockSz;
	ptSz = (int16_t)in.read_int16();
	if (ptSz < 0)
		ptSz = -ptSz;

	in.seek(block2start);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 2);
	uintptr_t block3start = in.tell() + blockSz;
	lineHeight = in.read_int16();
	base = in.read_int16();

	in.seek(block3start);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 3);
	uintptr_t block4start = in.tell() + blockSz;
	std::string texname;
	for (;;) {
		auto byte = in.read_byte();
		if (!byte)
			break;
		texname.push_back((char)byte);
	}

	in.seek(block4start);
	blockid = in.read_byte();
	blockSz = in.read_int32();
	ASSERT(blockid == 4);
	uint32_t numChars = blockSz / 20;
	for (uint32_t charIdx = 0; charIdx < numChars; charIdx++) {

		FontGlyph glyph;

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

		character_to_glyph.insert({id, glyph});
	}
	std::string texpath = StringUtils::get_directory(get_name()) + "/" + texname;
	font_texture = g_assets.find_sync_sptr<Texture>(texpath);
	return true;
}

Rect2d FontAsset::calc_text_size_no_wrap(std::string_view sv, const FontAsset* font) {
	ASSERT(font);
	int x = 0;
	int y = -font->base;
	for (char c : sv) {
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10; // empty character
		} else
			x += find->second.advance;
	}
	return Rect2d(0, y, x, font->lineHeight);
}
Rect2d FontAsset::calc_text_size(std::string_view sv, const FontAsset* font, int force_width) {
	ASSERT(font);

	if (force_width == -1)
		return calc_text_size_no_wrap(sv, font);

	std::string currentLine;
	std::string currentWord;

	int x = 0;
	int y = -font->base;
	for (char c : sv) {
		if (c == ' ' || c == '\n') {
			auto sz = calc_text_size_no_wrap((currentLine + currentWord).c_str(), font);
			if (sz.w > force_width) {
				x = glm::max((int)sz.w, x);
				y += font->lineHeight;
				currentLine = currentWord + " ";
			} else
				currentLine += currentWord + " ";
			currentWord.clear();
			if (c == '\n') {
				y += font->lineHeight;
				currentLine.clear();
			}
		} else
			currentWord += c;
	}
	if (!currentWord.empty()) {
		currentLine += currentWord;

		x = glm::max((int)calc_text_size_no_wrap(currentLine.c_str(), font).w, x);
		y += font->lineHeight;
	}

	return Rect2d(0, -font->base, x, (y + font->base));
}
