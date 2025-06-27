#ifndef ENVPROBE_H
#define ENVPROBE_H

#include "Shader.h"
#include "Framework/Util.h"
#include <string>
#include "Texture.h"

class EnvCubemap
{
public:
	uint32_t size;
	uint32_t original_cubemap=0;
	uint32_t irradiance_cm=0;
	uint32_t prefiltered_specular_cm=0;

	std::string hdr_file_name;
};

class BRDFIntegration
{
public:
	void run();
	uint32_t get_texture() {
		return lut_id;
	}
	void drawdebug();

	uint32_t lut_id;
	Shader integrate_shader;
	uint32_t fbo, depth;
	uint32_t quadvbo, quadvao;
private:
};

class Texture;
class EnviornmentMapHelper
{
public:
	static const int CUBEMAP_SIZE = 128;
	static const int MAX_MIP_ROUGHNESS;
	static const int BRDF_PREINTEGRATE_LUT_SIZE = 512;

	static EnviornmentMapHelper& get();
	void init();

	// convolutes a rendered cubemap
	void compute_specular_new(
		Texture* t	// in-out cubemap, scene drawn to mip level 0
		);

	// causes pipeline stall to read back texture
	void compute_irradiance_new(Texture* t, // in cubemap, scene draw to mip level 0
		glm::vec3 ambient_cube[6]		// out 6 vec3s representing irradiance of cubemap
	);


	BRDFIntegration integrator;
	glm::mat4 cubemap_projection;
	glm::mat4 cubemap_views[6];

private:
	program_handle prefilter_irradiance;
	program_handle prefilter_specular_new;

	uint32_t fbo,rbo;
	uint32_t vbo, vao;
};



#endif // ENVPROBE_H
