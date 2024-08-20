#include "ParticleEditorLocal.h"
#include "GameEnginePublic.h"
#include <SDL2/SDL.h>
#include "OsInput.h"
void ParticleEditorTool::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x = 0, y = 0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			camera.update_from_input(eng->get_input_state()->keys, x, y, glm::mat4(1.f));
		}
	}

	view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}


bool ParticleEditorTool::has_document_open() const
{
	return true;
}

void ParticleEditorTool::open_document_internal(const char* name, const char* arg)
{
}

void ParticleEditorTool::close_internal()
{
}

bool ParticleEditorTool::save_document_internal()
{
	return false;
}
void ParticleEditorTool::imgui_draw()
{

}