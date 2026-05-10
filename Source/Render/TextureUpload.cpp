// TextureUpload.cpp — Raw pixel data -> GPU texture upload, mipmap generation,
// and format helpers. Also contains development benchmark utilities.

#include "Texture.h"
#include "IGraphsDevice.h"
#include "Framework/Util.h"

#include "glad/glad.h"

#include "stb_image.h"

#include <algorithm>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Format helpers
// ---------------------------------------------------------------------------

enum class TextureMipLevel
{
	Highest,
	Medium,
	Low,
};

GraphicsTextureFormat to_format(int n, bool isfloat) {
	ASSERT(n >= 1 && n <= 4);
	using gtf = GraphicsTextureFormat;
	if (isfloat) {
		if (n == 4) return gtf::rgba16f;
		if (n == 3) return gtf::rgb16f;
	} else {
		if (n == 1) return gtf::r8;
		if (n == 2) return gtf::rg8;
		if (n == 3) return gtf::rgb8;
		if (n == 4) return gtf::rgba8;
	}
	ASSERT(0 && "unknown to_format combination");
	return gtf::rgb8;
}

// ---------------------------------------------------------------------------
// make_from_data — upload raw pixel data, generate mipmaps when not nearest
// ---------------------------------------------------------------------------

IGraphicsTexture* make_from_data(Texture* output, int x, int y, void* data, GraphicsTextureFormat informat,
                                 bool nearest_filtered = false) {
	ASSERT(output && data && x > 0 && y > 0);

	const int num_mip_maps = Texture::get_mip_map_count(x, y);
	auto create_gpu_texture = [&]() {
		CreateTextureArgs args;
		args.width        = x;
		args.height       = y;
		args.num_mip_maps = num_mip_maps;
		args.format       = informat;
		args.type         = GraphicsTextureType::t2D;
		if (nearest_filtered)
			args.sampler_type = GraphicsSamplerType::NearestDefault;
		else
			args.sampler_type = GraphicsSamplerType::AnisotropyDefault;
		return IGraphicsDevice::inst->create_texture(args);
	};
	IGraphicsTexture* ptr = create_gpu_texture();
	ASSERT(!ptr->is_compressed()); // compressed data must take the DDS path

	const int size = 0;

	ptr->sub_image_upload(0, 0, 0, x, y, size, data);
	if (!nearest_filtered) {
		// fixme
		glGenerateTextureMipmap(ptr->get_internal_handle());
	}

	glCheckError();
	return ptr;
}

// ---------------------------------------------------------------------------
// Benchmark utilities (dev-only, never called in shipping builds)
// ---------------------------------------------------------------------------

#include "Framework/Files.h"
#include "MaterialLocal.h"

struct StreamTextureData
{
	// cached_header mirrors the on-disk DDS header for mip-level management
	// ddsFileHeader_t cached_header{};  // type is in TextureDDS.cpp TU
	int   num_mips_allocated = 0;
	int   num_mips_loaded    = 0;
	float mip_lod_factor     = 0.0f;

	int wanted_mip_level = -1;
};

void texture_loading_benchmark() {
	ASSERT(0); // dev-only: never call in release
	std::vector<std::byte> filedata;
	filedata.reserve(10'000'000);
	double start = GetTime();
	double last  = 0.0;
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		last = now - start;
		start = now;
	};

	const char* hdd_path = "E:/Users/charl/Downloads/WorkLight02_Base_Color.dds";
	auto run_benchmark = [&](int max_mips) {
		printf("mips: %d\n", max_mips);
		IFilePtr ptr = FileSys::open_read(hdd_path, FileSys::FULL_SYSTEM);
		print_time("	open file");
		print_time("	read file");
		Texture dummy;
		print_time("	load to opengl");
		return dummy.gpu_ptr;
	};

	for (int i = 12; i >= 1; i -= 1) {
		run_benchmark(i);
	}

	auto try_copy = [&]() {
		IGraphicsTexture* src = run_benchmark(5);
		const glm::ivec2 size = src->get_size();
		const int mips = src->get_num_mips();
		texhandle new_t{};
		print_time("copying_texture...");
		glCreateTextures(GL_TEXTURE_2D, 1, &new_t);
		glTextureStorage2D(new_t, mips, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, size.x, size.y);
		print_time("	glTextureStorage2D...");

		for (int mip = 0; mip < mips; ++mip) {
			int mipWidth  = std::max(1, size.x >> mip);
			int mipHeight = std::max(1, size.y >> mip);
			glCopyImageSubData(src->get_internal_handle(), GL_TEXTURE_2D, mip, 0, 0, 0,
			                   new_t, GL_TEXTURE_2D, mip, 0, 0, 0, mipWidth, mipHeight, 1);
		}
		print_time("	copying done.");
	};
	try_copy();
	printf("%f\n", float(last));
	__debugbreak();
}

void benchmark_run() {
	ASSERT(0); // dev-only placeholder
#if 0
	const char* dds_file = "Data\\Textures\\Cttexturenavy.dds";
	const char* png_file = "Data\\Textures\\Cttexturenavy.png";
	const int RUNS = 500;
	Texture tex;
	std::vector<char> data;
	double start = GetTime();
	{
		for (int i = 0; i < RUNS; i++) {
			int x, y, channels;
			void* data = stbi_load(png_file, &x, &y, &channels, 0);
			make_from_data(&tex, x, y, data, to_format(channels, false));
			stbi_image_free(data);
		}
	}
	double end = GetTime();
	printf("PNG: %f\n", float(end - start));
	start = GetTime();
	for (int i = 0; i < RUNS; i++) {
		std::ifstream infile(dds_file, std::ios::binary);
		infile.seekg(0, std::ios::end);
		size_t len = infile.tellg();
		data.resize(len);
		infile.seekg(0);
		infile.read(data.data(), len);
		load_dds_file(&tex, (uint8_t*)data.data(), len);
	}
	end = GetTime();
	printf("DDS: %f\n", float(end - start));
#endif
}
