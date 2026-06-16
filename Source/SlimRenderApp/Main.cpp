
#include <cstdio>
#include "Framework/Util.h"
#include <SDL3/SDL.h>
#include "Render/IGraphicsDevice.h"
SDL_Window* window = nullptr;
#include "Framework/MeshBuilder.h"
#include "Framework/MeshBuilderImpl.h"

int main(int argc, char** argv)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
		sys_print(Error, "init sdl failed: %s\n", SDL_GetError());
		exit(-1);
	}
	const char* title = "SlimRenderApp";
	window = SDL_CreateWindow(title, 800, 600, SDL_WINDOW_RESIZABLE);
	if (!window) {
		sys_print(Error, "create sdl window failed: %s\n", SDL_GetError());
		exit(-1);
	}

	//if (use_dx11)
	gfx_init_dx11(window);
	gfx().set_vsync(true);
	//else
		// Backend creates the GL context, loads glad, takes over swap-interval.
	//	gfx_init_opengl(window);

	MeshBuilder mb;
	MeshBuilderDD dd;
	mb.Begin();

	for (;;)
	{
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				return 0;
			}
		}


		auto* framebuffer = gfx().acquire_swapchain_texture();
		RenderPassState pass;
		ColorTargetInfo back(framebuffer);
		back.wants_clear = true;
		back.clear_color = glm::vec4(1, 0, 0.5, 0);
		auto colorinfos = {back};
		gfx().set_render_pass(RenderPassState{.color_infos=colorinfos});
	
		gfx().submit_and_present();
	}



	printf("hello world\n");
	return 0;
}