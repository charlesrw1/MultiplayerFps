#include "EnvProbe.h"
#include "glad/glad.h"
#include <glm/ext.hpp>
#include "Texture.h"
#include "MeshBuilder.h"
#include "Draw.h"

using glm::vec3;
using glm::mat4;
static const float cube_verts[] = {
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
};

EnviornmentMapHelper& EnviornmentMapHelper::get()
{
    static EnviornmentMapHelper instance = EnviornmentMapHelper();
    return instance;
}
void EnviornmentMapHelper::init()
{

    Shader::compile(&to_cubemap_shader, "Helpers/EqrtCubemapV.txt", "Helpers/EqrtCubemapF.txt");
    Shader::compile(&prefilter_irradiance, "Helpers/EqrtCubemapV.txt", "Helpers/PrefilterIrradianceF.txt");
    Shader::compile(&prefilter_specular, "Helpers/EqrtCubemapV.txt", "Helpers/PrefilterSpecularF.txt");

    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CUBEMAP_SIZE, CUBEMAP_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    cubemap_projection = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 100.f);
    cubemap_views[0] = glm::lookAt(vec3(0), vec3(1, 0, 0), vec3(0, -1, 0));
    cubemap_views[1] = glm::lookAt(vec3(0), vec3(-1, 0, 0), vec3(0, -1, 0));
    cubemap_views[2] = glm::lookAt(vec3(0), vec3(0, 1, 0), vec3(0, 0, 1));
    cubemap_views[3] = glm::lookAt(vec3(0), vec3(0, -1, 0), vec3(0, 0, -1));
    cubemap_views[4] = glm::lookAt(vec3(0), vec3(0, 0, 1), vec3(0, -1, 0));
    cubemap_views[5] = glm::lookAt(vec3(0), vec3(0, 0, -1), vec3(0, -1, 0));

    integrator.run();
}

EnvCubemap EnviornmentMapHelper::create_from_file(std::string hdr_file)
{
	auto pos = hdr_file.rfind('.');
	if (pos == std::string::npos || hdr_file.substr(pos) != ".hdr") {
		printf("Cubemap conversion needs .hdr file\n");
        return {};
	}


    Texture* hdr_image = mats.find_texture(hdr_file.c_str(), true, true);
	if (!hdr_image)
        return {};

    EnvCubemap cubemap;
	cubemap.hdr_file_name = hdr_file;
    cubemap.size = CUBEMAP_SIZE;
    
    uint32_t cm_id;

    glGenTextures(1, &cm_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm_id);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, CUBEMAP_SIZE, CUBEMAP_SIZE, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    to_cubemap_shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_image->gl_id);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, CUBEMAP_SIZE, CUBEMAP_SIZE);
    glBindVertexArray(vao);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < 6; i++) {
        to_cubemap_shader.set_mat4("ViewProj", cubemap_projection*cubemap_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm_id, 0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cubemap.original_cubemap = cm_id;

    return cubemap;
}

void EnviornmentMapHelper::convolute_irradiance(EnvCubemap* env_map)
{
    assert(env_map);

    uint32_t cm_id;

    glGenTextures(1, &cm_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm_id);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    prefilter_irradiance.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env_map->original_cubemap);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 32, 32);
    glBindVertexArray(vao);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < 6; i++) {
        prefilter_irradiance.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm_id, 0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    env_map->irradiance_cm = cm_id;
}

void EnviornmentMapHelper::compute_specular(EnvCubemap* env_map)
{
    assert(env_map);

    uint32_t cm_id;
    glGenTextures(1, &cm_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cm_id);
    for (int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    prefilter_specular.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env_map->original_cubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindVertexArray(vao);
    glDisable(GL_CULL_FACE);
    int size = 128;
    for (int mip = 0; mip < MAX_MIP_ROUGHNESS; mip++) 
    {
        glViewport(0, 0, size, size);
        float roughness = (float)mip / (MAX_MIP_ROUGHNESS - 1);
        prefilter_specular.set_float("roughness", roughness);

        for (int i = 0; i < 6; i++) {
            prefilter_irradiance.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cm_id,mip);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        size >>= 1;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    env_map->prefiltered_specular_cm = cm_id;
}
void EnviornmentMapHelper::convolute_irradiance_array(uint32_t input_cubemap, int input_size, uint32_t output_array, int output_index, int output_size)
{
    prefilter_irradiance.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, input_cubemap);


    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, output_size, output_size);
    glBindVertexArray(vao);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < 6; i++) {
        prefilter_irradiance.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_array, 0, 6 * output_index + i);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glEnable(GL_CULL_FACE);
}
void EnviornmentMapHelper::compute_specular_array(uint32_t input_cubemap, int input_size, uint32_t output_array, int output_index, int output_size)
{
    prefilter_specular.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP,input_cubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindVertexArray(vao);
    glDisable(GL_CULL_FACE);
    int size = output_size;
    for (int mip = 0; mip < MAX_MIP_ROUGHNESS; mip++)
    {
        glViewport(0, 0, size, size);
        float roughness = (float)mip / (MAX_MIP_ROUGHNESS - 1);
        prefilter_specular.set_float("roughness", roughness);

        for (int i = 0; i < 6; i++) {
            prefilter_irradiance.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, output_array, mip, 6 * output_index + i);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        size /= 2;
    }
}
static const float quad_verts[] =
{
    -1,-1,0,
    1,-1,0,
    1,1,0,

    -1,-1,0,
    -1,1,0,
    1,1,0
};

void BRDFIntegration::drawdebug()
{
    return;

    MeshBuilder mb;
    mb.Begin();
    mb.Push2dQuad(vec2(0, 0), vec2(1, 1));
    mb.End();

    Renderer& d = draw;
    glCheckError();

    glCheckError();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCheckError();
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError();
    glDisable(GL_CULL_FACE);
    Shader& s = d.shader_list[d.S_TEXTURED];
    s.use();
    Texture* t = mats.find_texture("frog.jpg");
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, t->gl_id);
    glCheckError();
    s.set_mat4("Model", mat4(1));
    glCheckError();
    s.set_mat4("ViewProj", mat4(1));
    mb.Draw(GL_TRIANGLES);
    glCheckError();

    glEnable(GL_CULL_FACE);
    glCheckError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mb.Free();
    glCheckError();

}
void BRDFIntegration::run()
{
    Shader::compile (&integrate_shader, "MbTexturedV.txt", "Helpers/PreIntegrateF.txt");

    glGenBuffers(1, &quadvbo);
    glGenVertexArrays(1, &quadvao);
    glBindVertexArray(quadvao);
    glBindBuffer(GL_ARRAY_BUFFER, quadvbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &lut_id);
    glBindTexture(GL_TEXTURE_2D, lut_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, BRDF_PREINTEGRATE_LUT_SIZE, BRDF_PREINTEGRATE_LUT_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lut_id, 0);


    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, BRDF_PREINTEGRATE_LUT_SIZE, BRDF_PREINTEGRATE_LUT_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer not complete!\n");


    MeshBuilder mb;
    mb.Begin();
    mb.Push2dQuad(vec2(-1, 1), vec2(2, -2));
    mb.End();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCheckError();
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError();
    glDisable(GL_CULL_FACE);
    Shader& s = integrate_shader;
    s.use();

    s.set_mat4("Model", mat4(1));
    glCheckError();
    s.set_mat4("ViewProj", mat4(1));
    mb.Draw(GL_TRIANGLES);
    glCheckError();

    glEnable(GL_CULL_FACE);
    glCheckError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mb.Free();
    glCheckError();
}