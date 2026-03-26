
// header only stuff

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef APIENTRY
#define TINYEXR_IMPLEMENTATION

#include "tinyexr.h"
#undef APIENTRY

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define USE_CGLTF


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include"stb_image_write.h"


// TODO: This is an example of a library function
void fnExternal()
{
}
