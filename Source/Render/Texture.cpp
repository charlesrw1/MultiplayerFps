// Texture.cpp — Texture asset lifecycle: loading, GPU installation, system
// texture helpers, and editor metadata registration.
//
// DDS format parsing lives in TextureDDS.cpp.
// Raw pixel upload and mipmap generation live in TextureUpload.cpp.

#include "Texture.h"
#include <vector>

#include "glm/glm.hpp"

#include "stb_image.h"

#include "glad/glad.h"
#include "Framework/Util.h"

#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Assets/AssetRegistry.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Framework/Config.h"
#undef APIENTRY

#include "tinyexr.h"
#undef APIENTRY

#include "IGraphsDevice.h"

// TextureEditor.cpp
extern bool compile_texture_asset(const std::string& gamepath, IAssetLoadingInterface*, Color32& outColor);

// ---------------------------------------------------------------------------
// Editor-only: PNG/HDR write wrappers and asset metadata registration
// ---------------------------------------------------------------------------
#ifdef EDITOR_BUILD
#include "stb_image_write.h"
int write_png_wrapper(const char* filename, int w, int h, int comp, const void* data, int stride_in_bytes) {
	ASSERT(filename && w > 0 && h > 0 && comp > 0 && data);
	return stbi_write_png(filename, w, h, comp, data, stride_in_bytes);
}
int write_hdr_wrapper(const char* filename, int w, int h, int comp, const float* data) {
	ASSERT(filename && w > 0 && h > 0 && comp > 0 && data);
	return stbi_write_hdr(filename, w, h, comp, data);
}

class TextureAssetMetadata : public AssetMetadata
{
public:
	TextureAssetMetadata() {
		extensions.push_back("dds");
		extensions.push_back("hdr");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return {227, 39, 39}; }

	virtual std::string get_type_name() const override { return "Texture"; }

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const override {
		filepaths.push_back("_white");
		filepaths.push_back("_black");
		filepaths.push_back("_flat_normal");
	}

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Texture::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(TextureAssetMetadata);
#endif

// ---------------------------------------------------------------------------
// Forward declarations for functions defined in sibling translation units
// ---------------------------------------------------------------------------

// TextureDDS.cpp
bool load_dds_file(Texture* output, IGraphicsTexture*& out_ptr, uint8_t* buffer, int len);

// TextureUpload.cpp
IGraphicsTexture* make_from_data(Texture* output, int x, int y, void* data, GraphicsTextureFormat informat,
                                 bool nearest_filtered = false);
GraphicsTextureFormat to_format(int n, bool isfloat);

// ---------------------------------------------------------------------------
// Texture asset lifecycle
// ---------------------------------------------------------------------------

using std::string;
using std::vector;

#include "GameEnginePublic.h"

void Texture::move_construct(IAsset* _src) {
	ASSERT(_src);
	assert(!eng->get_is_in_overlapped_period());
	Texture* src = (Texture*)_src;
	uninstall();
	assert(!gpu_ptr);
	gpu_ptr  = src->gpu_ptr;
	loaddata = std::move(src->loaddata);
	src->gpu_ptr = nullptr; // steal without triggering uninstall
#ifdef EDITOR_BUILD
	simplifiedColor    = src->simplifiedColor;
	hasSimplifiedColor = src->hasSimplifiedColor;
#endif
}

glm::ivec2 Texture::get_size() const {
	ASSERT(true); // gpu_ptr may be null before post_load
	if (gpu_ptr) {
		return gpu_ptr->get_size();
	}
	return {};
}

texhandle Texture::get_internal_render_handle() const {
	ASSERT(true); // valid to call with null gpu_ptr (returns 0)
	if (gpu_ptr)
		return gpu_ptr->get_internal_handle();
	return 0;
}

ConfigVar disable_texture_loads("disable_texture_loads", "0", CVAR_BOOL | CVAR_DEV, "");

void Texture::post_load() {
	ASSERT(true); // loaddata may be null if load failed
	if (did_load_fail())
		return;

	if (disable_texture_loads.get_bool()) {
		const uint8_t missing_tex[] = {230, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 230, 0, 255, 255};
		auto create_defeault = [](IGraphicsTexture*& handle, const uint8_t* data) -> void {
			CreateTextureArgs args;
			args.num_mip_maps = 1;
			args.width        = 2;
			args.height       = 2;
			args.format       = GraphicsTextureFormat::rgba8;
			args.sampler_type = GraphicsSamplerType::LinearDefault;

			handle = IGraphicsDevice::inst->create_texture(args);
			handle->sub_image_upload(0, 0, 0, 2, 2, sizeof(uint8_t) * 4 * 4, data);
		};
		create_defeault(gpu_ptr, missing_tex);
		return;
	}

	auto user = loaddata.get();

	int& x          = user->x;
	int& y          = user->y;
	auto& data      = user->data;
	auto& filedata  = user->filedata;

	if (user->isDDSFile)
		load_dds_file(this, gpu_ptr, filedata.data(), filedata.size());
	else
		gpu_ptr = make_from_data(this, x, y, data, to_format(user->channels, user->is_float),
		                         user->wantsNearestFiltering);

	if (data)
		stbi_image_free(data);

	loaddata.reset();
}

extern ConfigVar developer_mode;

bool Texture::load_asset(IAssetLoadingInterface* loading) {
	const auto& path = get_name();
	ASSERT(!path.empty());
	assert(path != "_white" &&
	       path != "_black"); // default textures must be initialized before anything else
	if (disable_texture_loads.get_bool())
		return true;

#ifdef EDITOR_BUILD
	if (developer_mode.get_bool() && !force_nearest) {
		bool good = compile_texture_asset(path, loading, this->simplifiedColor);
		if (good)
			this->hasSimplifiedColor = true;
	}
#endif

	auto file = FileSys::open_read_game(path);
	if (!file) {
		return false;
	}

	loaddata = std::make_unique<LoadData>();
	auto user     = loaddata.get();
	int& x        = user->x;
	int& y        = user->y;
	auto& data    = user->data;
	auto& filedata = user->filedata;
	auto& is_float = user->is_float;
	auto& channels = user->channels;

	if (path.find("/_nearest") != std::string::npos)
		user->wantsNearestFiltering = true; // hack moment
	user->wantsNearestFiltering |= force_nearest;

	user->filedata.resize(file->size());
	file->read(user->filedata.data(), user->filedata.size());

	if (path.find(".dds") != std::string::npos) {
		user->isDDSFile = true;
		return true;
	} else if (path.find(".hdr") != std::string::npos) {
		data     = stbi_loadf_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		filedata = {};
		is_float = true;
	} else if (path.find(".exr") != std::string::npos) {
		float*      out_data = nullptr;
		const char* err      = nullptr;
		int res = LoadEXRFromMemory(&out_data, &x, &y, filedata.data(), filedata.size(), &err);
		if (res != TINYEXR_SUCCESS) {
			sys_print(Error, "Texture::load_asset: couldnt load .exr: %s\n", err ? err : "<unknown>");
			return false;
		}
		is_float = true;
		channels = 4;
		filedata = {};
		data     = out_data;
	} else {
		data     = stbi_load_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		filedata = {};
		is_float = false;
	}
	file->close();

	if (data == nullptr) {
		loaddata.reset();
		return false;
	}

	return true;
}

void Texture::uninstall() {
	ASSERT(true); // safe to call with null gpu_ptr
	safe_release(gpu_ptr);
}

void Texture::update_specs_ptr(IGraphicsTexture* ptr) {
	ASSERT(ptr);
	this->gpu_ptr = ptr;
}

Texture* Texture::install_system(const std::string& path) {
	ASSERT(!path.empty());
	Texture* t = new Texture;
	g_assets.install_system_asset(t, path);
	return t;
}

Texture* Texture::force_load_for_ui(const string& name) {
	ASSERT(!name.empty());
	Texture* t       = Texture::install_system(name);
	t->set_loaded_manually_unsafe(name);
	t->force_nearest = true; // hack
	const bool good  = t->load_asset(nullptr /*hope nobody uses this*/);
	if (good)
		t->post_load();
	return t;
}

Texture::Texture() {}
Texture::~Texture() {}

#include "Assets/AssetDatabase.h"
Texture* Texture::load(const std::string& path) {
	ASSERT(!path.empty());
	return g_assets.find_sync<Texture>(path).get();
}
