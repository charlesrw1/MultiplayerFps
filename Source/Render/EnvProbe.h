#ifndef ENVPROBE_H
#define ENVPROBE_H

#include "Shader.h"
#include "Framework/Util.h"
#include <string>
#include "Texture.h"
const int CUBEMAP_SIZE = 128;
const int MAX_MIP_ROUGHNESS = get_mip_map_count(CUBEMAP_SIZE, CUBEMAP_SIZE);
const int BRDF_PREINTEGRATE_LUT_SIZE = 512;

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
	static EnviornmentMapHelper& get();
	void init();

	EnvCubemap create_from_file(std::string hdr_file);
	void convolute_irradiance(EnvCubemap* env_map);
	void compute_specular(EnvCubemap* env_map);

	void convolute_irradiance_array(uint32_t input_cubemap, int input_size, uint32_t output_array, int output_index, int output_size);
	void compute_specular_array(uint32_t input_cubemap, int input_size, uint32_t output_array, int output_index, int output_size);


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
	Shader to_cubemap_shader;
	Shader prefilter_irradiance;
	Shader prefilter_specular;

	Shader prefilter_specular_new;

	uint32_t fbo,rbo;
	uint32_t vbo, vao;
};



#endif // ENVPROBE_H
