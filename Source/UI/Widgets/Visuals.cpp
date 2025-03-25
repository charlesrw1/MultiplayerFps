#include "Visuals.h"

#include "Render/Texture.h"
namespace gui {
Box::~Box() {

}
Box::Box() {
	recieve_mouse = guiMouseFilter::Pass;
}
}