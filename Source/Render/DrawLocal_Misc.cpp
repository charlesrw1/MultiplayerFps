#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h" // GL_* constants still referenced; gl* calls go through IGraphicsDevice
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>

#include "UI/GUISystemPublic.h" // for GuiSystemPublic::paint
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h" // FIXME
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"

const GLenum MODEL_INDEX_TYPE_GL = GL_UNSIGNED_SHORT;

//#pragma optimize("", off)

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;

Renderer draw;
RendererPublic* idraw = &draw;

// HOLY CONFIG VARS
ConfigVar enable_vsync("r.enable_vsync", "1", CVAR_BOOL, "enable/disable vsync");
ConfigVar shadow_quality_setting("r.shadow_setting", "0", CVAR_INTEGER, "csm shadow quality", 0, 3);
ConfigVar enable_bloom("r.bloom", "1", CVAR_BOOL, "enable/disable bloom");
ConfigVar enable_volumetric_fog("r.vol_fog", "0", CVAR_BOOL, "enable/disable volumetric fog");
ConfigVar enable_ssao("r.ssao", "1", CVAR_BOOL, "enable/disable screen space ambient occlusion");
ConfigVar use_halfres_reflections("r.halfres_reflections", "1", CVAR_BOOL, "");
ConfigVar dont_use_mdi("r.dont_use_mdi", "0", CVAR_BOOL | CVAR_DEV,
					   "disable multidrawindirect and use drawelements instead");
// 12mb arena
ConfigVar renderer_memory_arena_size("r.mem_arena_size", "12000000", CVAR_INTEGER | CVAR_UNBOUNDED,
									 "size of the renderers memory arena in bytes");

ConfigVar r_taa_enabled("r.taa", "1", CVAR_BOOL, "enable temporal anti aliasing");

static const int MAX_TAA_SAMPLES = 16;
ConfigVar r_taa_samples("r.taa_samples", "4", CVAR_INTEGER, "", 2, MAX_TAA_SAMPLES);
ConfigVar r_taa_32f("r.taa_32f", "0", CVAR_BOOL, "use 32 bit scene motion buffer instead of 16 bit");

// basically:
// diffuse_ao = pow(ao, ssao.intensity)
// specular_ao = pow(diffuse_ao,r_specular_ao_intensity)

ConfigVar r_specular_ao_intensity("r.specular_ao_intensity", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar r_debug_skip_build_scene_data("r.debug_skip_build_scene_data", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_gbuffer("r_skip_gbuffer", "0", CVAR_BOOL, "");

ConfigVar force_render_cubemaps("r.force_cubemap_render", "0", CVAR_BOOL | CVAR_DEV,
								"force cubemaps to re-render, treated like a flag and not a setting");

ConfigVar r_drawterrain("r.drawterrain", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of terrain");
ConfigVar r_force_hide_ui("r.force_hide_ui", "0", CVAR_BOOL, "disable ui drawing");
ConfigVar test_thumbnail_model("test_thumbnail_model", "", CVAR_DEV, "");

ConfigVar r_drawdecals("r.drawdecals", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of decals");

ConfigVar thumbnail_fov("thumbnail_fov", "60", CVAR_FLOAT | CVAR_UNBOUNDED, "");

ConfigVar debug_sun_shadow("r.debug_csm", "0", CVAR_BOOL | CVAR_DEV, "debug csm shadow rendering");
ConfigVar debug_specular_reflection("r.debug_specular", "0", CVAR_BOOL | CVAR_DEV, "debug specular lighting");
ConfigVar r_no_indirect("r.no_indirect", "0", CVAR_BOOL, "");

ConfigVar r_no_meshbuilders("r_no_meshbuilders", "0", CVAR_BOOL | CVAR_DEV, "");
// 128 bones * 100 characters * 2 (double bffer) =
ConfigVar r_skinned_mats_bone_buffer_size("r.skinned_mats_bone_buffer_size", "25600",
										  CVAR_INTEGER | CVAR_UNBOUNDED | CVAR_READONLY, "");

ConfigVar r_better_depth_batching("r.better_depth_batching", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_no_batching("r.no_batching", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_ignore_depth_shader("r_ignore_depth_shader", "0", CVAR_BOOL | CVAR_DEV, "...");

ConfigVar enable_gl_debug_output("enable_gl_debug_output", "1", CVAR_BOOL, "");
ConfigVar r_taa_jitter_test("r.taa_jitter_test", "0", CVAR_INTEGER, "", 0, 4);
ConfigVar r_normal_shaded_debug("r.normal_shaded_debug", "1", CVAR_BOOL, "");
ConfigVar log_shader_compiles("log_shader_compiles", "0", CVAR_BOOL, "");

ConfigVar r_debug_mode("r.debug_mode", "0", CVAR_INTEGER | CVAR_DEV,
					   "render debug mode, see Draw.cpp for DEBUG_x values, 0 to disable", 0, 200);

ConfigVar r_drawfog("r.drawfog", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of fog");

ConfigVar ddgi_test("dt", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar ddgi_rt("ddrt", "0", CVAR_BOOL | CVAR_DEV, "");

extern void build_frustum_for_cascade(Frustum& f, int index);

RenderWindowBackend* RenderWindowBackend::inst = nullptr;
class RenderWindowBackendLocal : public RenderWindowBackend
{
public:
	int id_counter = 1;

	std::vector<UIDrawCmdUnion> drawCmds;

	handle<RenderWindow> register_window() { return {1}; }
	void update_window(handle<RenderWindow> handle, RenderWindow& data) final {
		assert(handle.id == 1);
		// return;
		drawCmds = data.get_draw_cmds();
		mb_draw_data.init_from(data.meshbuilder);
		this->view_proj = data.view_mat;
	}
	virtual void remove_window(handle<RenderWindow> handle) final { assert(handle.id == 1); }

	void render() {
		// return;
		gfx().bind_uniform_buffer_base(0, draw.ubo.current_frame);
		auto& device = draw.get_device();
		for (int i = 0; i < (int)drawCmds.size(); i++) {
			UIDrawCmdUnion& cmd = drawCmds[i];
			switch (cmd.type) {
			case UiDrawCmdType::DrawCall: {
				UiDrawCallCmd& drawCmd = cmd.drawCmd;
				gfx().draw_elements_base_vertex(GraphicsPrimitiveType::Triangles, drawCmd.index_count,
												VertexInputIndexType::uint32,
												drawCmd.index_start * (int)sizeof(int),
												drawCmd.base_vertex);
				draw.stats.total_draw_calls++;
			} break;
			case UiDrawCmdType::SetScissor: {
				UiSetScissorCmd& r = cmd.scissorCmd;
				gfx().set_scissor(r.x, r.y, r.w, r.h);
			} break;
			case UiDrawCmdType::ClearScissor:
				gfx().disable_scissor();
				break;
			case UiDrawCmdType::SetPipeline: {
				MaterialInstance* mat = cmd.pipelineCmd.mat;

				assert(mat->get_master_material()->usage == MaterialUsage::UI);
				RenderPipelineState pipe;
				pipe.backface_culling = true;
				pipe.blend = mat->get_master_material()->blend;
				pipe.cull_front_face = false;
				pipe.depth_testing = false;
				pipe.depth_writes = false;
				pipe.program = draw.get_prog_man().get_obj(matman.get_mat_shader(nullptr, mat, 0));
				pipe.vao = mb_draw_data.vao;
				device.set_pipeline(pipe);

				gpu::MasterUIVertPushConsts pcv{};
				pcv.UIViewProj = view_proj;
				gfx().push_vertex_constants(0, &pcv, sizeof(pcv));

				auto& texs = mat->impl->get_textures();
				for (int i = 0; i < (int)texs.size(); i++)
					device.bind_texture(i, texs[i]->gpu_ptr);
			} break;
			case UiDrawCmdType::SetTexture:
				if (cmd.textureCmd.tex)
					device.bind_texture(cmd.textureCmd.binding, cmd.textureCmd.tex->gpu_ptr);
				else
					device.bind_texture(cmd.textureCmd.binding, draw.white_texture);
				break;
			case UiDrawCmdType::SetModelMatrix:
				break;

			default:
				break;
			}
			draw.stats.total_draw_calls++;
		}

		gfx().disable_scissor();

		device.reset_state_cache();
	}

private:
	glm::mat4 view_proj{};
	MeshBuilderDD mb_draw_data;
};

class TaaManager
{
public:
	TaaManager() { generateHaltonSequence(MAX_TAA_SAMPLES, jitters); }

	void start_frame() { index = (index + 1) % r_taa_samples.get_integer(); }
	glm::vec2 get_last_frame_jitter(int w, int h) const {
		int previndex = index - 1;
		if (previndex < 0)
			previndex = r_taa_samples.get_integer() - 1;
		return calc_jitter(previndex, w, h);
	}
	glm::vec2 calc_frame_jitter(int width, int height) const { return calc_jitter(index, width, height); }
	glm::mat4 add_jitter_to_projection(const glm::mat4& inproj, glm::vec2 jitter) const {
		glm::mat4 matrix = inproj;
		matrix[2][0] += jitter.x;
		matrix[2][1] += jitter.y;

		return matrix;
	}

private:
	glm::vec2 calc_jitter(int the_index, int width, int height) const {
		ASSERT(width > 0 && height > 0);
		auto jit = jitters[the_index]; // [0,1]
		jit = jit - glm::vec2(0.5);	   //[-1/2,1/2]
		return glm::vec2(jit.x / width, jit.y / height);
	}

	static float radicalInverse(int base, int index) {
		ASSERT(base >= 2);
		float result = 0.0;
		float fraction = 1.0 / base;
		while (index > 0) {
			result += (index % base) * fraction;
			index /= base;
			fraction /= base;
		}
		return result;
	}
	static void generateHaltonSequence(int numPoints, glm::vec2* sequence) {
		ASSERT(numPoints > 0 && sequence != nullptr);
		const int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
		const int dimension = 2;
		for (int d = 0; d < dimension; ++d) {
			int base = primes[d];
			for (int i = 0; i < numPoints; ++i) {
				sequence[i][d] = radicalInverse(base, i + 1);
			}
		}
	}

	glm::vec2 jitters[MAX_TAA_SAMPLES];
	int index = 0;
};
static TaaManager r_taa_manager;

// BuildSceneData_CpuFast batch/draw implementation -> DrawLocal_BatchScene.cpp
// BuildSceneData_CpuFast culling/shadow/removal implementation -> DrawLocal_CullShadow.cpp
BuildSceneData_CpuFast* BuildSceneData_CpuFast::inst = nullptr;
