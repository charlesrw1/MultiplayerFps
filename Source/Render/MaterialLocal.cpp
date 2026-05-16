#include "MaterialLocal.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "AssetCompile/Someutils.h"
#include "DrawLocal.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <array>
#include "glad/glad.h"
#include "IGraphsDevice.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetDatabase.h"
#include "Framework/StringUtils.h"
#include "imgui.h"
#include "EditorPopups.h"

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

MaterialManagerLocal matman;
MaterialManagerPublic* imaterials = &matman;

ConfigVar material_print_debug("material_print_debug", "1", CVAR_DEV | CVAR_BOOL, "");

MulticastDelegate<MaterialInstance*> MaterialInstance::on_material_loaded;

// ---------------------------------------------------------------------------
// MaterialInstance – public interface
// ---------------------------------------------------------------------------

MaterialInstance::MaterialInstance() {}

MaterialInstance::~MaterialInstance() {
	if (impl && impl->is_dynamic_material) {
		matman.free_dynamic_material(this);
	}
#ifdef _DEBUG
	std::vector<IAsset*> mats;
	g_assets.get_assets_of_type(mats, &MaterialInstance::StaticType);
	for (auto mat : mats) {
		if (mat == this)
			continue;
		assert(mat);
		auto matPtr = (MaterialInstance*)mat;
		if (!matPtr->impl)
			continue;
		assert(!impl || matPtr->impl->masterMaterial.get() != this);
	}
	std::vector<IAsset*> mods;
	g_assets.get_assets_of_type(mods, &Model::StaticType);
	for (auto mod : mods) {
		auto modPtr = (Model*)mod;
		assert(mod);
		for (int i = 0; i < modPtr->get_num_materials(); i++) {
			assert(modPtr->get_material(i) != this);
		}
	}
#endif
}

const MasterMaterialImpl* MaterialInstance::get_master_material() const {
	ASSERT(impl);
	return impl->get_master_impl();
}

bool MaterialInstance::is_this_a_master_material() const {
	return impl && impl->masterMaterial;
}

bool MaterialInstance::is_this_a_dynamic_material() const {
	return impl && impl->is_dynamic_material;
}

bool MaterialInstance::load_asset() {
	ASSERT(!impl); // reload path always uninstalls first; initial path starts with impl null
	impl = std::make_unique<MaterialImpl>();
	bool good = impl->load_from_file(this);
	assert(!good || (impl && impl->is_valid()));
	if (!good)
		impl.reset();
	return good;
}

void MaterialInstance::uninstall() {
	matman.remove_from_dirty_list_if_it_is(this);
	matman.free_material_instance(this);
	impl.reset();
}

void MaterialInstance::post_load() {
	if (did_load_fail())
		return;
	assert(impl);
	ASSERT(impl->is_valid());

	const bool is_reload = first_post_load_done;
	impl->post_load(this);
	first_post_load_done = true;

	if (is_reload) {
		// Cascade: re-load every child instance whose master is *this*.
		// Address of *this* is stable across reload so child->impl->masterMaterial
		// pointer comparison still works (the shared_ptr's stored ptr is unchanged).
		std::vector<IAsset*> ar;
		g_assets.get_assets_of_type(ar, &MaterialInstance::StaticType);
		for (auto a : ar) {
			if (a == this)
				continue;
			auto* mi = static_cast<MaterialInstance*>(a);
			if (mi->impl && mi->impl->masterMaterial.get() == this)
				g_assets.reload(mi);
		}
		matman.on_reloaded_material(this);
	}

	MaterialInstance::on_material_loaded.invoke(this);
}

MaterialInstance* MaterialInstance::load(const std::string& path) {
	ASSERT(!path.empty());
	return g_assets.find<MaterialInstance>(path).get();
}

MaterialInstance* MaterialInstance::alloc_dynamic_mat(MaterialInstance* from) {
	ASSERT(from);
	return matman.create_dynmaic_material_unsafier(from);
}

void MaterialInstance::free_dynamic_mat(MaterialInstance* mat) {
	ASSERT(mat);
	return matman.free_dynamic_material(mat);
}

// ---------------------------------------------------------------------------
// MaterialInstance – parameter setters
// ---------------------------------------------------------------------------

void MaterialInstance::set_float_parameter(StringName name, float f) {
	ASSERT(impl);
	if (auto p = impl->find_param_type(name, MatParamType::Float)) {
		p->scalar = f;
		matman.add_to_dirty_list(this);
	} else {
		sys_print(Error, "couldnt find parameter for set_float_parameter\n");
	}
}

void MaterialInstance::set_floatvec_parameter(StringName name, glm::vec4 f) {
	ASSERT(impl);
	if (auto p = impl->find_param_type(name, MatParamType::FloatVec)) {
		p->vector = f;
		matman.add_to_dirty_list(this);
	} else {
		sys_print(Error, "couldnt find parameter for set_floatvec_parameter\n");
	}
}

void MaterialInstance::set_u8vec_parameter(StringName name, Color32 f) {
	ASSERT(impl);
	if (auto p = impl->find_param_type(name, MatParamType::Vector)) {
		p->color32 = f.to_uint();
		matman.add_to_dirty_list(this);
	} else {
		sys_print(Error, "couldnt find parameter for set_u8vec_parameter\n");
	}
}

void MaterialInstance::set_tex_parameter(StringName name, const Texture* t) {
	ASSERT(impl);
	if (!t)
		return;

	if (auto p = impl->find_param_type(name, MatParamType::Texture2D)) {
		p->tex = g_assets.find_sync_sptr<Texture>(t->get_name());
		matman.add_to_dirty_list(this);
	} else {
		sys_print(Error, "couldnt find parameter for set_tex_parameter\n");
	}
}

// ---------------------------------------------------------------------------
// MaterialImpl – texture hash helper
// ---------------------------------------------------------------------------

int MaterialImpl::get_texture_id_hash() {
	ASSERT(self);
	if (texture_id_hash.has_value())
		return texture_id_hash.value();
	// Material is still in matman.dirty_list — texture_bindings slots may be null
	// and gpu_buffer_offset may not be assigned yet. Force the flush now so the
	// hash AND the GPU material buffer match what callers (sort key, batching
	// cache) will use this frame. flush_dirty_material assigns texture_id_hash.
	matman.flush_dirty_material(self);
	ASSERT(texture_id_hash.has_value());
	return texture_id_hash.value();
}

// ---------------------------------------------------------------------------
// DynamicMaterialDeleter
// ---------------------------------------------------------------------------

void DynamicMaterialDeleter::operator()(MaterialInstance* mat) const {
	ASSERT(mat && mat->impl->is_dynamic_material);
	matman.free_dynamic_material(mat);
}

// ---------------------------------------------------------------------------
// AllMaterialTable
// ---------------------------------------------------------------------------

AllMaterialTable::AllMaterialTable(int max_materials) : allocator(max_materials) {
	ASSERT(max_materials > 0);
	all_mats.resize(max_materials, nullptr);
}

void AllMaterialTable::register_material(MaterialInstance* mat) {
	ASSERT(mat);
	assert(mat->impl->gpu_buffer_offset == MaterialImpl::INVALID_MAPPING);
	const int index = allocator.allocate();
	assert(!all_mats.at(index));
	all_mats.at(index) = mat;
	const int ofs = index * MATERIAL_SIZE / 4;
	mat->impl->gpu_buffer_offset = ofs;
}

void AllMaterialTable::unregister_material(MaterialInstance* mat) {
	ASSERT(mat);
	const int gpu_buf_ofs = mat->impl->gpu_buffer_offset;
	assert(gpu_buf_ofs != MaterialImpl::INVALID_MAPPING);
	const int byteIndex = mat->impl->get_material_index_from_buffer_ofs();
	allocator.free(byteIndex);
	assert(all_mats.at(byteIndex) == mat);
	all_mats.at(byteIndex) = nullptr;
	mat->impl->gpu_buffer_offset = MaterialImpl::INVALID_MAPPING;
}

const std::vector<MaterialInstance*>& AllMaterialTable::get_all_mat_array() const {
	return all_mats;
}

// ---------------------------------------------------------------------------
// BitmapAllocator
// ---------------------------------------------------------------------------

BitmapAllocator::BitmapAllocator(int size) {
	ASSERT(size > 0);
	int bitmap_sz = size / 64;
	if (size % 64 != 0)
		bitmap_sz += 1;
	materialBitmapAllocator.resize(bitmap_sz);
	this->max_ids = size;
}

int BitmapAllocator::allocate() {
	ASSERT(!materialBitmapAllocator.empty());
	for (int i = 0; i < (int)materialBitmapAllocator.size(); i++) {
		if (materialBitmapAllocator[i] == UINT64_MAX)
			continue;
		for (int bit = 0; bit < 64; bit++) {
			if ((materialBitmapAllocator[i] & (1ull << bit)) == 0) {
				materialBitmapAllocator[i] |= (1ull << bit);
				return i * 64 + bit;
			}
		}
		ASSERT(0);
	}
	Fatalf("allocate_material_instance: out of memory\n");
	return 0;
}

void BitmapAllocator::free(int id) {
	ASSERT(id >= 0 && id < max_ids);
	int bitmapIndex = id / 64;
	int bitIndex = id % 64;
	materialBitmapAllocator.at(bitmapIndex) &= ~(1ull << bitIndex);
}

// ---------------------------------------------------------------------------
// TextureBindingHasher
// ---------------------------------------------------------------------------

int TextureBindingHasher::get_texture_hash_id_for_material(MaterialImpl* mat) {
	ASSERT(mat);
	if (mat->texture_bindings.empty())
		return NO_TEXTURE_ID;
	opt<int> existing = find_existing(mat->texture_bindings);
	if (existing.has_value())
		return *existing;
	return insert_new(mat->texture_bindings);
}

bool TextureBindingHasher::are_arrays_equal(const InlineVec<Texture*, 6>& v1, const std::vector<Texture*>& v2) {
	if (v1.size() != v2.size())
		return false;
	for (int i = 0; i < (int)v1.size(); i++) {
		if (v1[i] != v2[i])
			return false;
	}
	return true;
}

int TextureBindingHasher::insert_new(const std::vector<Texture*> bindings) {
	ASSERT(!bindings.empty());
	Texture* first = bindings.at(0);
	assert(first);
	const int this_id = current_texture_hash_id++;
	HashItem item;
	item.id = this_id;
	for (auto t : bindings)
		item.textures.push_back(t);
	table[first].push_back(item);
	return this_id;
}

opt<int> TextureBindingHasher::find_existing(const std::vector<Texture*> bindings) {
	ASSERT(true); // bindings may be empty — handled by caller
	if (bindings.empty())
		return NO_TEXTURE_ID;
	Texture* first = bindings.at(0);
	assert(first);
	const HashItemVec* items = MapUtil::get_opt(table, first);
	if (items) {
		for (int i = 0; i < (int)items->size(); i++) {
			const HashItem* item = &(*items)[i];
			const bool are_equal = are_arrays_equal(item->textures, bindings);
			if (are_equal)
				return item->id;
		}
	}
	return std::nullopt;
}

// ---------------------------------------------------------------------------
// DynamicMaterialAllocator
// ---------------------------------------------------------------------------

MaterialInstance* DynamicMaterialAllocator::allocate_dynamic() {
	ASSERT(true); // no precondition on caller side
	auto it = free_dynamic_ptrs.begin();
	MaterialInstance* first = nullptr;
	if (it != free_dynamic_ptrs.end()) {
		first = *it;
		free_dynamic_ptrs.remove(first);
	} else {
		first = new MaterialInstance;
	}

	outstanding_dynamic_mats += 1;

	ASSERT(first);
	first->init_runtime_unmanaged("*dm");
	assert(first->is_valid_to_use());
	return first;
}

void DynamicMaterialAllocator::free_dynamic(MaterialInstance* mat) {
	ASSERT(mat && mat->is_this_a_dynamic_material());
	ASSERT(free_dynamic_ptrs.find(mat) == nullptr);

	outstanding_dynamic_mats -= 1;
	ASSERT(outstanding_dynamic_mats >= 0);
	mat->uninstall();
	free_dynamic_ptrs.insert(mat);
}
