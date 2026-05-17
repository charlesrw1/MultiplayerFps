#if 1
// OpenGLShaderImpl — owns a compiled+linked GL program. Factory functions
// declared in OpenGlDeviceLocal.h. Phase 1.7a: bodies delegate to the
// legacy Shader::compile* helpers; the source-loader and compile/link GL
// calls move into this TU in phase 1.7b.
#include "OpenGlDeviceLocal.h"
#include "Shader.h"
#include "glad/glad.h"

class OpenGLShaderImpl : public IGraphicsShader
{
public:
	uint32_t program_id = 0;

	explicit OpenGLShaderImpl(uint32_t id) : program_id(id) {}
	~OpenGLShaderImpl() override {
		if (program_id != 0)
			glDeleteProgram(program_id);
	}

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return program_id; }
};

static IGraphicsShader* wrap_or_fail(const Shader& temp, ShaderResult result) {
	if (result != ShaderResult::SHADER_SUCCESS || temp.ID == 0)
		return nullptr;
	return new OpenGLShaderImpl(temp.ID);
}

IGraphicsShader* opengl_create_shader_vert_frag(const std::string& vert_path,
												const std::string& frag_path,
												const std::string& defines) {
	Shader temp{};
	auto result = Shader::compile(&temp, vert_path, frag_path, defines);
	return wrap_or_fail(temp, result);
}

IGraphicsShader* opengl_create_shader_vert_frag_geo(const std::string& vert_path,
													const std::string& frag_path,
													const std::string& geo_path,
													const std::string& defines) {
	Shader temp{};
	bool ok = Shader::compile(temp, vert_path, frag_path, geo_path, defines);
	return wrap_or_fail(temp, ok ? ShaderResult::SHADER_SUCCESS
								 : ShaderResult::SHADER_COMPILE_FAIL);
}

IGraphicsShader* opengl_create_shader_compute(const std::string& compute_path,
											  const std::string& defines) {
	Shader temp{};
	auto result = Shader::compute_compile(&temp, compute_path, defines);
	return wrap_or_fail(temp, result);
}

IGraphicsShader* opengl_create_shader_single_file(const std::string& shared_path,
												  const std::string& defines) {
	Shader temp{};
	auto result = Shader::compile_vert_frag_single_file(&temp, shared_path, defines);
	return wrap_or_fail(temp, result);
}

IGraphicsShader* opengl_create_shader_single_file_tess(const std::string& shared_path,
													   const std::string& defines) {
	Shader temp{};
	auto result = Shader::compile_vert_frag_tess_single_file(&temp, shared_path, defines);
	return wrap_or_fail(temp, result);
}
#endif
