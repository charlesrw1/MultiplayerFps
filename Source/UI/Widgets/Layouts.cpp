#include "UI/GuiSystemLocal.h"
#include "UI/Widgets/Layouts.h"

void guiVBox::on_mouse_scroll(const SDL_MouseWheelEvent& wheel)
{
	printf("scroll vertbox\n");
	start -= wheel.y*20;
}