#if 1
// OpenGLTextureImpl — GPU texture creation, format mapping, upload, and destruction.
// Provides factory functions declared in OpenGlDeviceLocal.h.
#include "OpenGlDeviceLocal.h"

// ---------------------------------------------------------------------------
// Format helpers (also exposed as opengl_texture_format_to_str / opengl_filter_to_gl)
// ---------------------------------------------------------------------------
const char* opengl_texture_format_to_str(GraphicsTextureFormat fmt) {
	static const array enum_strs = {"r8",
									"rg8",
									"rgb8",
									"rgba8",
									"r16f",
									"rg16f",
									"rgb16f",
									"rgba16f",
									"r32f",
									"rg32f",
									"bc1",
									"bc1_srgb",
									"bc3",
									"bc4",
									"bc5",
									"bc6",
									"bc7",
									"bc7_srgb",
									"depth16f",
									"depth24f",
									"depth32f",
									"depth24stencil8",
									"r11f_g11f_b10f",
									"rgba16_snorm"};
	ASSERT((int)fmt >= 0 && (int)fmt < (int)enum_strs.size());
	return enum_strs[(int)fmt];
}

GLenum opengl_filter_to_gl(GraphicsFilterType type) {
	ASSERT((int)type >= 0);
	switch (type) {
	case GraphicsFilterType::Linear:       return GL_LINEAR;
	case GraphicsFilterType::Nearest:      return GL_NEAREST;
	case GraphicsFilterType::MipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
	}
	ASSERT(0 && "opengl_filter_to_gl: unknown filter type");
	return GL_LINEAR;
}

// ---------------------------------------------------------------------------
// OpenGLTextureImpl
// ---------------------------------------------------------------------------
class OpenGLTextureImpl : public IGraphicsTexture
{
public:
	// ---- Format conversion helpers ----------------------------------------
	static double get_bytes_per_pixel(GraphicsTextureFormat fmt) {
		ASSERT((int)fmt >= 0);
		switch (fmt) {
		case GraphicsTextureFormat::r8:              return 1;
		case GraphicsTextureFormat::rg8:             return 2;
		case GraphicsTextureFormat::rgb8:            return 4;
		case GraphicsTextureFormat::rgba8:           return 4;
		case GraphicsTextureFormat::r16f:            return 2;
		case GraphicsTextureFormat::rg16f:           return 4;
		case GraphicsTextureFormat::rgb16f:          return 8;
		case GraphicsTextureFormat::rgba16f:         return 8;
		case GraphicsTextureFormat::r32f:            return 4;
		case GraphicsTextureFormat::rg32f:           return 8;
		case GraphicsTextureFormat::bc1:             return 0.5;
		case GraphicsTextureFormat::bc3:             return 1;
		case GraphicsTextureFormat::bc4:             return 1;
		case GraphicsTextureFormat::bc5:             return 1;
		case GraphicsTextureFormat::bc6:             return 1; // fixme
		case GraphicsTextureFormat::bc7:             return 1;
		case GraphicsTextureFormat::bc7_srgb:        return 1;
		case GraphicsTextureFormat::depth24f:        return 4;
		case GraphicsTextureFormat::depth32f:        return 4;
		case GraphicsTextureFormat::depth24stencil8: return 4;
		case GraphicsTextureFormat::r11f_g11f_b10f:  return 4;
		case GraphicsTextureFormat::rgba16_snorm:    return 8;
		default: break;
		}
		return 0;
	}

	static int estimate_memory_usage(const CreateTextureArgs& args) {
		ASSERT(args.width > 0 && args.height > 0 && args.num_mip_maps >= 1);
		double bytes_per_pixel = get_bytes_per_pixel(args.format);
		int total = 0;
		int x = args.width;
		int y = args.height;
		for (int i = 0; i < args.num_mip_maps; i++) {
			total += x * y * bytes_per_pixel;
			x >>= 2;
			y >>= 2;
		}
		if (args.type == GraphicsTextureType::t2DArray)
			total = total * args.depth_3d;
		return total;
	}

	static GLenum to_type(GraphicsTextureType type) {
		ASSERT((int)type >= 0);
		switch (type) {
		case GraphicsTextureType::t2D:           return GL_TEXTURE_2D;
		case GraphicsTextureType::t2DArray:      return GL_TEXTURE_2D_ARRAY;
		case GraphicsTextureType::t3D:           return GL_TEXTURE_3D;
		case GraphicsTextureType::tCubemap:      return GL_TEXTURE_CUBE_MAP;
		case GraphicsTextureType::tCubemapArray: return GL_TEXTURE_CUBE_MAP_ARRAY;
		default: break;
		}
		ASSERT(0 && "OpenGLTextureImpl::to_type undefined");
		return GL_TEXTURE_2D;
	}

	static GLenum get_input_format(GraphicsTextureFormat fmt) {
		ASSERT((int)fmt >= 0);
		switch (fmt) {
		case GraphicsTextureFormat::r8:              return GL_RED;
		case GraphicsTextureFormat::rg8:             return GL_RG;
		case GraphicsTextureFormat::rgb8:            return GL_RGB;
		case GraphicsTextureFormat::rgba8:           return GL_RGBA;
		case GraphicsTextureFormat::r16f:            return GL_RED;
		case GraphicsTextureFormat::rg16f:           return GL_RG;
		case GraphicsTextureFormat::rgb16f:          return GL_RGB;
		case GraphicsTextureFormat::rgba16f:         return GL_RGBA;
		case GraphicsTextureFormat::r32f:            return GL_RED;
		case GraphicsTextureFormat::rg32f:           return GL_RG;
		case GraphicsTextureFormat::r11f_g11f_b10f:  return GL_RGB;
		case GraphicsTextureFormat::rgba16_snorm:    return GL_RGBA;
		case GraphicsTextureFormat::bc1:
		case GraphicsTextureFormat::bc3:
		case GraphicsTextureFormat::bc4:
		case GraphicsTextureFormat::bc5:
		case GraphicsTextureFormat::bc6:
		case GraphicsTextureFormat::bc7:
		case GraphicsTextureFormat::bc7_srgb:
		case GraphicsTextureFormat::depth16f:
		case GraphicsTextureFormat::depth24f:
		case GraphicsTextureFormat::depth32f:
		case GraphicsTextureFormat::depth24stencil8:
			return GL_RGB; // fixme, should be unused
		default: break;
		}
		ASSERT(0 && "OpenGLTextureImpl::get_input_format undefined");
		return GL_RED;
	}

	static GLenum to_format(GraphicsTextureFormat fmt) {
		ASSERT((int)fmt >= 0);
		switch (fmt) {
		case GraphicsTextureFormat::r8:              return GL_R8;
		case GraphicsTextureFormat::rg8:             return GL_RG8;
		case GraphicsTextureFormat::rgb8:            return GL_RGB8;
		case GraphicsTextureFormat::rgba8:           return GL_RGBA8;
		case GraphicsTextureFormat::r16f:            return GL_R16F;
		case GraphicsTextureFormat::rg16f:           return GL_RG16F;
		case GraphicsTextureFormat::rgb16f:          return GL_RGB16F;
		case GraphicsTextureFormat::rgba16f:         return GL_RGBA16F;
		case GraphicsTextureFormat::r32f:            return GL_R32F;
		case GraphicsTextureFormat::rg32f:           return GL_RG32F;
		case GraphicsTextureFormat::bc1:             return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		case GraphicsTextureFormat::bc1_srgb:        return GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
		case GraphicsTextureFormat::bc3:             return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		case GraphicsTextureFormat::bc4:             return GL_COMPRESSED_RED_RGTC1;
		case GraphicsTextureFormat::bc5:             return GL_COMPRESSED_RG_RGTC2;
		case GraphicsTextureFormat::bc6:             return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		case GraphicsTextureFormat::bc7:             return GL_COMPRESSED_RGBA_BPTC_UNORM;
		case GraphicsTextureFormat::bc7_srgb:        return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
		case GraphicsTextureFormat::depth16f:        return GL_DEPTH_COMPONENT16;
		case GraphicsTextureFormat::depth24f:        return GL_DEPTH_COMPONENT24;
		case GraphicsTextureFormat::depth32f:        return GL_DEPTH_COMPONENT32F;
		case GraphicsTextureFormat::depth24stencil8: return GL_DEPTH24_STENCIL8;
		case GraphicsTextureFormat::r11f_g11f_b10f:  return GL_R11F_G11F_B10F;
		case GraphicsTextureFormat::rgba16_snorm:    return GL_RGBA16_SNORM;
		}
		ASSERT(0 && "OpenGLTextureImpl: unknown texture format");
		return GL_RGB8;
	}

	// ---- Construction / destruction ----------------------------------------
	int mem_usage = 0;
	OpenGLTextureImpl() = default; // sentinel; no GL resources allocated

	OpenGLTextureImpl(const CreateTextureArgs& args) {
		ASSERT(args.width > 0 && args.height > 0 && args.num_mip_maps >= 1);
		opengl_stats.all_textures.insert(static_cast<IGraphicsTexture*>(this));
		mem_usage = estimate_memory_usage(args);
		total_gfx_mem_usage += mem_usage;
		auto type = to_type(args.type);
		glCreateTextures(type, 1, &id);
		const int x = args.width;
		const int y = args.height;
		this->width  = args.width;
		this->height = args.height;
		this->mips   = args.num_mip_maps;
		this->float_input_is_16f = args.float_input_is_16f;
		my_type          = args.type;
		my_fmt           = args.format;
		internal_format_gl = to_format(args.format);
		if (args.type == GraphicsTextureType::t2DArray ||
			args.type == GraphicsTextureType::tCubemapArray ||
			args.type == GraphicsTextureType::t3D)
			glTextureStorage3D(id, args.num_mip_maps, internal_format_gl, x, y, args.depth_3d);
		else
			glTextureStorage2D(id, args.num_mip_maps, internal_format_gl, x, y);

		switch (args.sampler_type) {
		case GraphicsSamplerType::AnisotropyDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
			break;
		case GraphicsSamplerType::LinearNoMipmaps:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		case GraphicsSamplerType::LinearDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		case GraphicsSamplerType::NearestDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			break;
		case GraphicsSamplerType::LinearClamped:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			break;
		case GraphicsSamplerType::NearestClamped:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		case GraphicsSamplerType::CsmShadowmap: {
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float bordercolor[] = {1.0, 1.0, 1.0, 1.0};
			glTextureParameterfv(id, GL_TEXTURE_BORDER_COLOR, bordercolor);
		} break;
		case GraphicsSamplerType::AtlasShadowmap:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			break;
		case GraphicsSamplerType::CubemapDefault:
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		case GraphicsSamplerType::DepthPyramid:
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			// glTextureParameteri(id, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MIN);
			break;
		default:
			break;
		}
	}

	~OpenGLTextureImpl() override {
		// glDeleteTextures silently ignores id == 0 (sentinel case)
		glDeleteTextures(1, &id);
		total_gfx_mem_usage -= mem_usage;
		opengl_stats.all_textures.remove(static_cast<IGraphicsTexture*>(this));
	}

	// ---- IGraphicsTexture interface ----------------------------------------
	texhandle id = 0;
	uint32_t get_internal_handle() override { return id; }

	void sub_image_upload(int level, int x, int y, int w, int h, int size, const void* data) override {
		ASSERT(w > 0 && h > 0);
		if (is_compressed())
			glCompressedTextureSubImage2D(id, level, x, y, w, h, internal_format_gl, size, data);
		else {
			const GLenum type = get_input_type();
			const GLenum input_fmt = get_input_format(my_fmt);
			glTextureSubImage2D(id, level, x, y, w, h, input_fmt, type, data);
		}
	}

	void sub_image_upload_3d(int z, int level, int x, int y, int w, int h, int size, const void* data) final {
		ASSERT(w > 0 && h > 0);
		if (is_compressed())
			glCompressedTextureSubImage3D(id, level, x, y, z, w, h, 1, internal_format_gl, size, data);
		else {
			const GLenum type = get_input_type();
			const GLenum input_fmt = get_input_format(my_fmt);
			glTextureSubImage3D(id, level, x, y, z, w, h, 1, input_fmt, type, data);
		}
	}

	void release() override { delete this; }

	bool is_compressed() const {
		int first = (int)GraphicsTextureFormat::bc1;
		int last  = (int)GraphicsTextureFormat::bc7_srgb;
		int fmt_i = (int)my_fmt;
		return fmt_i >= first && fmt_i <= last;
	}

	int get_compressed_stride() const final {
		ASSERT(is_compressed());
		if (my_fmt == GraphicsTextureFormat::bc1 || my_fmt == GraphicsTextureFormat::bc1_srgb ||
			my_fmt == GraphicsTextureFormat::bc4)
			return 8;
		return 16;
	}

	GLenum get_input_type() const {
		ASSERT(id != 0);
		if (my_fmt == GraphicsTextureFormat::r11f_g11f_b10f)
			return GL_UNSIGNED_INT_10F_11F_11F_REV;
		using gtf = GraphicsTextureFormat;
		// SNORM texture storage takes normalized int16 input data, not float.
		if (my_fmt == gtf::rgba16_snorm)
			return GL_SHORT;
		if (my_fmt == gtf::rg16f || my_fmt == gtf::r16f || my_fmt == gtf::rgb16f)
			return float_input_is_16f ? GL_HALF_FLOAT : GL_FLOAT;
		if (is_float_type())
			return GL_FLOAT;
		return GL_UNSIGNED_BYTE;
	}

	bool is_float_type() const {
		ASSERT((int)my_fmt >= 0);
		using gtf = GraphicsTextureFormat;
		auto types = {
			gtf::r16f, gtf::rg16f, gtf::rgb16f, gtf::rgba16f, gtf::r32f, gtf::rg32f,
			gtf::depth16f, gtf::depth24f, gtf::depth32f, gtf::r11f_g11f_b10f,
		};
		for (auto t : types)
			if (t == my_fmt)
				return true;
		return false;
	}

	glm::ivec2 get_size() const override { return {width, height}; }
	GraphicsTextureFormat get_texture_format() const override { return my_fmt; }
	GraphicsTextureType   get_texture_type()   const override { return my_type; }
	int get_num_mips() const override { return mips; }
	int get_mem_usage() const override { return mem_usage; }

	void set_mip_range(int base, int max) override {
		ASSERT(base >= 0 && max >= base);
		glTextureParameteri(id, GL_TEXTURE_BASE_LEVEL, base);
		glTextureParameteri(id, GL_TEXTURE_MAX_LEVEL, max);
	}

	void generate_mipmaps() override {
		ASSERT(id != 0);
		glGenerateTextureMipmap(id);
	}

	void download(int mip, int layer, void* dest, int dest_size_bytes) override {
		ASSERT(dest != nullptr && dest_size_bytes > 0);
		GLenum fmt = 0;
		GLenum type = 0;
		switch (my_fmt) {
		case GraphicsTextureFormat::depth32f:
		case GraphicsTextureFormat::depth24f:
		case GraphicsTextureFormat::depth16f:
			fmt = GL_DEPTH_COMPONENT; type = GL_FLOAT; break;
		case GraphicsTextureFormat::rgba8:
			fmt = GL_RGBA; type = GL_UNSIGNED_BYTE; break;
		case GraphicsTextureFormat::rgb8:
			fmt = GL_RGB; type = GL_UNSIGNED_BYTE; break;
		case GraphicsTextureFormat::rgb16f:
			fmt = GL_RGB; type = GL_FLOAT; break;
		case GraphicsTextureFormat::rgba16f:
			fmt = GL_RGBA; type = GL_FLOAT; break;
		case GraphicsTextureFormat::rg16f:
		case GraphicsTextureFormat::rg32f:
			fmt = GL_RG; type = GL_FLOAT; break;
		case GraphicsTextureFormat::r11f_g11f_b10f:
			fmt = GL_RGB; type = GL_FLOAT; break;
		default:
			ASSERT(!"IGraphicsTexture::download: unsupported format (extend mapping)");
			return;
		}
		if (layer < 0) {
			glGetTextureImage(id, mip, fmt, type, dest_size_bytes, dest);
		} else {
			const int w = std::max(1, width  >> mip);
			const int h = std::max(1, height >> mip);
			glGetTextureSubImage(id, mip, 0, 0, layer, w, h, 1, fmt, type,
								 dest_size_bytes, dest);
		}
	}

	void clear_image() final {
		ASSERT(!is_compressed());
		ASSERT(my_fmt != GraphicsTextureFormat::depth32f);
		ASSERT(my_fmt != GraphicsTextureFormat::depth24f);
		ASSERT(my_fmt != GraphicsTextureFormat::depth16f);
		if (is_float_type()) {
			float clear_values[] = {0.f, 0.f, 0.f, 0.f};
			glClearTexImage(id, 0, get_input_format(my_fmt), GL_FLOAT, clear_values);
		} else {
			uint8_t clear_values[] = {0, 0, 0, 0};
			glClearTexImage(id, 0, get_input_format(my_fmt), GL_UNSIGNED_BYTE, clear_values);
		}
	}

	GraphicsTextureType   my_type{};
	GraphicsTextureFormat my_fmt{};
	GLenum internal_format_gl{};
	int  width  = 0;
	int  height = 0;
	int  mips   = 0;
	bool float_input_is_16f = false;
};

// ---------------------------------------------------------------------------
// Factory functions (declared in OpenGlDeviceLocal.h)
// ---------------------------------------------------------------------------
IGraphicsTexture* opengl_make_swapchain_sentinel() {
	ASSERT(true); // always safe; returns a default-constructed sentinel
	return new OpenGLTextureImpl(); // id == 0, no GL resources
}

IGraphicsTexture* opengl_create_texture(const CreateTextureArgs& args) {
	ASSERT(args.width > 0 && args.height > 0 && args.num_mip_maps >= 1);
	return new OpenGLTextureImpl(args);
}
#endif
