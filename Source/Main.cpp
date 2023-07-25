#include <SDL2/SDL.h>
#include "glad/glad.h"
#include "stb_image.h"
#include <cstdio>
#include <vector>
#include <string>

#include "Shader.h"
#include "Texture.h"
#include "MathLib.h"
#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "Model.h"
#include "MeshBuilder.h"
#include "Util.h"

using glm::vec3;
using glm::vec2;
using glm::mat4;
using glm::mat3;
using glm::vec4;
using glm::ivec2;
using glm::uvec2;
using glm::ivec3;

using glm::dot;
using glm::cross;
using glm::normalize;
using glm::length;

const float PI = 3.1415926536;
const float TWOPI = PI * 2.f;
const float HALFPI = PI * 0.5f;
const float INV_PI = 1.f / PI;
const float SQRT2 = 1.41421362;
const float INV_SQRT2 = 1 / SQRT2;



bool CheckGlErrorInternal_(const char* file, int line)
{
	GLenum error_code = glGetError();
	bool has_error = 0;
	while (error_code != GL_NO_ERROR)
	{
		has_error = true;
		const char* error_name = "Unknown error";
		switch (error_code)
		{
		case GL_INVALID_ENUM:
			error_name = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE:
			error_name = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:
			error_name = "GL_INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:
			error_name = "GL_STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:
			error_name = "GL_STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:
			error_name = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default:
			break;
		}
		printf("%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
}

SDL_Window* window = nullptr;
SDL_GLContext context;
int vid_width = 800;
int vid_height = 600;
bool keyboard[SDL_NUM_SCANCODES];
int mouse_delta_x = 0;
int mouse_delta_y = 0;
int scroll_delta = 0;

const float fov = glm::radians(75.f);


void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(1);
}

bool CreateWindow()
{
	if (window)
		return true;

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		SDLError("SDL init failed");
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	uint32_t win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;// | SDL_WINDOW_FULLSCREEN;

	window = SDL_CreateWindow("CsRemake",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, vid_width, vid_height, win_flags);
	if (window == nullptr) {
		SDLError("SDL failed to create window");
		return false;
	}

	context = SDL_GL_CreateContext(window);
	printf("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	printf("Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(1);

	return true;
}

void Shutdown()
{
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

class FlyCamera
{
public:
	vec3 position = vec3(0);
	vec3 front = vec3(1, 0, 0);
	vec3 up = vec3(0, 1, 0);

	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	void UpdateFromInput(bool keys[], int mouse_dx, int mouse_dy, int scroll);
	void UpdateVectors() {
		front = AnglesToVector(pitch, yaw);
	}
	mat4 GetViewMatrix() const {
		return glm::lookAt(position, position + front, up);
	}
};

void FlyCamera::UpdateFromInput(bool keys[], int mouse_dx, int mouse_dy, int scroll)
{
	int xpos, ypos;
	xpos = mouse_dx;
	ypos = mouse_dy;

	float x_off = xpos;
	float y_off = ypos;
	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;

	yaw += x_off;
	pitch -= y_off;

	if (pitch > HALFPI - 0.01)
		pitch = HALFPI - 0.01;
	if (pitch < -HALFPI + 0.01)
		pitch = -HALFPI + 0.01;

	if (yaw > TWOPI) {
		yaw -= TWOPI;
	}
	if (yaw < 0) {
		yaw += TWOPI;
	}
	UpdateVectors();

	vec3 right = cross(up, front);
	if (keys[SDL_SCANCODE_W])
		position += move_speed * front;
	if (keys[SDL_SCANCODE_S])
		position -= move_speed * front;
	if (keys[SDL_SCANCODE_A])
		position += right * move_speed;
	if (keys[SDL_SCANCODE_D])
		position -= right * move_speed;
	if (keys[SDL_SCANCODE_Z])
		position += move_speed * up;
	if (keys[SDL_SCANCODE_X])
		position -= move_speed * up;

	move_speed += (move_speed * 0.5) * scroll;
	if (abs(move_speed) < 0.000001)
		move_speed = 0.0001;
}

FlyCamera fly_cam;
Shader simple;
Shader textured;
Shader animated;
Texture* mytexture;
Model* m = nullptr;
bool update_camera = false;

void Update()
{
	if(update_camera)
		fly_cam.UpdateFromInput(keyboard, mouse_delta_x, mouse_delta_y, scroll_delta);
}

void Render()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	mat4 perspective = glm::perspective(fov, (float)vid_width / vid_height, 0.01f, 100.0f);

	textured.use();
	textured.set_mat4("ViewProj", perspective*fly_cam.GetViewMatrix());
	textured.set_mat4("Model", mat4(1));
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mytexture->gl_id);

	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(vec2(-1),vec2(2),vec2(0),vec2(1),COLOR_WHITE);
	mb.End();
	mb.Draw(GL_TRIANGLES);
	
	animated.use();
	animated.set_mat4("ViewProj", perspective * fly_cam.GetViewMatrix());
	animated.set_mat4("Model", mat4(1));
	animated.set_mat4("InverseModel", mat4(1));

	glDisable(GL_CULL_FACE);
	for (int i = 0; i < m->parts.size(); i++)
	{
		MeshPart* part = &m->parts[i];
		glBindVertexArray(part->vao);
		glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}
	glCheckError();

	SDL_GL_SwapWindow(window);

}
void InitGlState()
{
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);
	glEnable(GL_MULTISAMPLE);
	glDepthFunc(GL_LEQUAL);
}

int main(int argc, char** argv)
{
	CreateWindow();
	printf("hello world");
	mytexture = FindOrLoadTexture("test2.jpg");
	Shader::compile(&simple, "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&textured, "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&animated, "AnimBasicV.txt", "AnimBasicF.txt", "ANIMATED");
	m = FindOrLoadModel("CT.glb");
	ASSERT(mytexture != nullptr);
	ASSERT(glCheckError() == 0);
	InitGlState();
	for(;;)
	{
		mouse_delta_x = mouse_delta_y = scroll_delta = 0;
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				Shutdown();
				return 1;
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				keyboard[event.key.keysym.scancode] = event.key.type == SDL_KEYDOWN;
				break;
			case SDL_MOUSEMOTION:
				mouse_delta_x += event.motion.xrel;
				mouse_delta_y += event.motion.yrel;
				break;
			case SDL_MOUSEWHEEL:
				scroll_delta += event.wheel.y;
			case SDL_MOUSEBUTTONDOWN:
				SDL_SetRelativeMouseMode(SDL_TRUE);
				update_camera = true;
				break;
			case SDL_MOUSEBUTTONUP:
				SDL_SetRelativeMouseMode(SDL_FALSE);
				update_camera = false;
				break;

			}
		}
		Update();
		Render();
	}

	Shutdown();

	return 0;
}