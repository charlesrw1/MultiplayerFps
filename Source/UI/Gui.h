#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/LuaColor.h"
#include "Framework/Rect2d.h"
class Texture;

class Gui : public ClassBase
{
public:
	CLASS_BODY(Gui);
	REF static void set_color(float r, float g, float b, float a);
	REF static void rectangle(int x, int y, int w, int h);
	REF static void rectangle_outline(int x, int y, int w, int h, int thickness);
	REF static void circle(int x, int y, int radius, int segments);
	REF static void line(int x1, int y1, int x2, int y2, int thickness);
	REF static void image(Texture* tex, int x, int y, int w, int h);
	REF static void print(std::string text, int x, int y);
	REF static lRect measure_text(std::string text);
	REF static lRect get_screen_size();

private:
	static Color32 current_color;
};
