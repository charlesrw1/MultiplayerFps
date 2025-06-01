#include "Visuals.h"

#include "Render/Texture.h"

guiBox::~guiBox() {

}
guiBox::guiBox() {
	recieve_mouse = guiMouseFilter::Pass;
}