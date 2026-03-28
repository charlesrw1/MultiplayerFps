#pragma once

#include "Render/DrawPublic.h"

struct Render_lists_cpufast
{
	IGraphicsBuffer* glinst_to_inst{}; // object indirection
	IGraphicsBuffer* cmdbuf{};		   // cmd buffer
	std::span<int> md_counts;		   // size = batches.size()
};

struct Render_Lists;
class Render_Pass;
struct Render_Level_Params
{

	enum Pass_Type
	{
		OPAQUE,
		FORWARD_PASS,
		DEPTH,
		SHADOWMAP
	};
	Render_Level_Params(const View_Setup& view, Render_Lists* render_list, Render_Pass* render_pass, Pass_Type type)
		: view(view), rl(render_list), rp(render_pass),

		pass(type) {}

	View_Setup view;

	float offset_poly_units = 1.1;

	Render_Lists* rl = nullptr;
	Render_Pass* rp = nullptr;

	Render_lists_cpufast* rl_cpufast = nullptr;

	Pass_Type pass = OPAQUE;
	bool draw_viewmodel = false;
	bool is_probe_render = false;
	bool is_water_reflection_pass = false;

	bool is_wireframe_pass = false;
	bool wireframe_secondpass = false;

	bool upload_constants = false;
	bufferhandle provied_constant_buffer = 0;

	// for cascade shadow map ortho!
	bool wants_non_reverse_z = false;
};
