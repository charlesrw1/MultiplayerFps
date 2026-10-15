#include "TerrainComponent.h"
#include "Game/Components/CameraComponent.h"
#include "Game/EditorAddMenu.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/ModelManager.h"
#include "Render/MaterialPublic.h"
#include "Render/Texture.h"
#include "Render/IGraphicsDevice.h"
#include "Assets/AssetDatabase.h"
#include "Physics/Physics2.h"
#include "Framework/Files.h"
#include "Framework/Serializer.h"
#include <algorithm>
#include <cmath>

namespace {
struct TerrainFileHeader { char magic[8] = {'C','S','T','E','R','R','1',0}; uint32_t resolution = 0; float size = 0.f; };
constexpr int MIN_RESOLUTION = 9;
constexpr int MAX_RESOLUTION = 255;

std::shared_ptr<MaterialInstance> terrain_master() {
	return g_assets.find_sync_sptr<MaterialInstance>("eng/terrain.mm");
}
Texture* make_runtime_texture(const char* name, int resolution, GraphicsTextureFormat format) {
	Texture* texture = Texture::install_system(name);
	CreateTextureArgs args;
	args.width = resolution;
	args.height = resolution;
	args.format = format;
	args.num_mip_maps = 1;
	args.sampler_type = GraphicsSamplerType::LinearClamped;
	texture->update_specs_ptr(gfx().create_texture(args));
	return texture;
}
}

REGISTER_EDITOR_ADD_MENU_ENTRY("Environment/Terrain", "TerrainComponent", true);

TerrainComponent::TerrainComponent() { set_call_init_in_editor(true); }
TerrainComponent::~TerrainComponent() {
	for (const auto& h : render_handles) ASSERT(!h.is_valid());
	ASSERT(!collision_actor);
}

void TerrainComponent::start() {
	ensure_data();
	load_data();
	set_ticking(true);
	rebuild_runtime();
}
void TerrainComponent::stop() {
	destroy_collision();
	destroy_render_meshes();
	terrain_material = nullptr;
}
// Component ticks can overlap renderer submission.  Never mutate Render_Scene
// proxies from here; schedule the regular render-sync callback instead.
void TerrainComponent::update() { sync_render_data(); }

// Terrain always occupies world X/Z at the origin.  Its owner transform is deliberately
// ignored: allowing it to move would desynchronise sampling, editing, and PhysX.
void TerrainComponent::on_changed_transform() { sync_render_data(); }

void TerrainComponent::serialize(Serializer& s) {
	Component::serialize(s);
	s.serialize("resolution", resolution); s.serialize("size", size); s.serialize("tile_size", material_tile_size);
	s.serialize("collision", collision_enabled); s.serialize("data", data_path);
	s.serialize_asset("material0", MaterialInstance::StaticType, (IAsset*&)material0);
	s.serialize_asset("material1", MaterialInstance::StaticType, (IAsset*&)material1);
	s.serialize_asset("material2", MaterialInstance::StaticType, (IAsset*&)material2);
	s.serialize_asset("material3", MaterialInstance::StaticType, (IAsset*&)material3);
	if (s.is_loading()) { ensure_data(); load_data(); textures_dirty = clipmap_dirty = true; }
	else if (data_dirty) save_data();
}

void TerrainComponent::set_resolution(int value) {
	value = glm::clamp(value, MIN_RESOLUTION, MAX_RESOLUTION);
	if (resolution == value) return;
	resolution = value; heights.clear(); weights.clear(); ensure_data();
	textures_dirty = clipmap_dirty = true; rebuild_runtime();
}
void TerrainComponent::set_size(float value) {
	value = glm::max(value, 1.f);
	if (size == value) return;
	size = value; textures_dirty = clipmap_dirty = true; rebuild_runtime();
}
void TerrainComponent::set_data_path(const std::string& value) { data_path = value; load_data(); textures_dirty = clipmap_dirty = true; rebuild_runtime(); }
void TerrainComponent::ensure_data() {
	const size_t count = (size_t)resolution * resolution;
	if (heights.size() != count) heights.assign(count, 0.f);
	if (weights.size() != count * 4) { weights.assign(count * 4, 0); for (size_t i=0;i<count;i++) weights[i*4] = 255; }
}
float TerrainComponent::height_at(int x, int z) const { x=glm::clamp(x,0,resolution-1); z=glm::clamp(z,0,resolution-1); return heights[index(x,z)]; }
float TerrainComponent::sample_height_local(float x, float z) const {
	float u=glm::clamp(x/size+.5f,0.f,1.f)*(resolution-1), v=glm::clamp(z/size+.5f,0.f,1.f)*(resolution-1);
	int x0=(int)u,z0=(int)v; float tx=u-x0,tz=v-z0;
	return glm::mix(glm::mix(height_at(x0,z0),height_at(x0+1,z0),tx),glm::mix(height_at(x0,z0+1),height_at(x0+1,z0+1),tx),tz);
}
float TerrainComponent::sample_height_world(float x, float z) const { return sample_height_local(x,z); }

void TerrainComponent::ensure_gpu_textures() {
	if (heightmap_texture && heightmap_texture->get_size().x == resolution) return;
	static uint64_t texture_generation = 0;
	const std::string stem = "_terrain_runtime_" + std::to_string((uintptr_t)this) + "_" + std::to_string(++texture_generation);
	heightmap_texture = make_runtime_texture((stem + "_height").c_str(), resolution, GraphicsTextureFormat::r32f);
	normalmap_texture = make_runtime_texture((stem + "_normal").c_str(), resolution, GraphicsTextureFormat::rgba8);
	splatmap_texture = make_runtime_texture((stem + "_splat").c_str(), resolution, GraphicsTextureFormat::rgba8);
	textures_dirty = true;
}
void TerrainComponent::upload_terrain_textures() {
	ensure_data(); ensure_gpu_textures();
	if (!textures_dirty) return;
	heightmap_texture->gpu_ptr->sub_image_upload(0,0,0,resolution,resolution,(int)(heights.size()*sizeof(float)),heights.data());
	std::vector<uint8_t> normal((size_t)resolution*resolution*4);
	const float step=size/(resolution-1);
	for (int z=0; z<resolution; ++z) for (int x=0; x<resolution; ++x) {
		glm::vec3 n=glm::normalize(glm::vec3(height_at(x-1,z)-height_at(x+1,z),2.f*step,height_at(x,z-1)-height_at(x,z+1)));
		const int o=index(x,z)*4; normal[o]=(uint8_t)glm::clamp((n.x*.5f+.5f)*255.f,0.f,255.f); normal[o+1]=(uint8_t)glm::clamp((n.y*.5f+.5f)*255.f,0.f,255.f); normal[o+2]=(uint8_t)glm::clamp((n.z*.5f+.5f)*255.f,0.f,255.f); normal[o+3]=255;
	}
	normalmap_texture->gpu_ptr->sub_image_upload(0,0,0,resolution,resolution,(int)normal.size(),normal.data());
	splatmap_texture->gpu_ptr->sub_image_upload(0,0,0,resolution,resolution,(int)weights.size(),weights.data());
	textures_dirty = false;
}

void TerrainComponent::update_terrain_material() {
	if (!terrain_material) {
		auto master=terrain_master();
		if (!master) return;
		terrain_material=master.get();
	}
	upload_terrain_textures();
	terrain_material->set_tex_parameter("HeightMap", heightmap_texture);
	terrain_material->set_tex_parameter("NormalMap", normalmap_texture);
	terrain_material->set_tex_parameter("SplatMap", splatmap_texture);
	terrain_material->set_float_parameter("TerrainSize", size);
	terrain_material->set_float_parameter("TileSize", material_tile_size);
	const Texture* fallback = Texture::load("_white");
	const MaterialInstance* layers[] = {material0, material1, material2, material3};
	const char* params[] = {"Layer0", "Layer1", "Layer2", "Layer3"};
	for (int i=0; i<4; ++i) {
		const Texture* layer = layers[i] ? layers[i]->get_tex_parameter("BaseColor") : nullptr;
		terrain_material->set_tex_parameter(params[i], layer ? layer : fallback);
	}
}

void TerrainComponent::build_render_meshes() {
	auto master=terrain_master();
	for (int level=0; level<CLIPMAP_LEVELS; ++level) {
		const float extent=glm::min(size, 32.f*std::pow(2.f,(float)level));
		const float previous_extent=level ? glm::min(size,32.f*std::pow(2.f,(float)(level-1))) : 0.f;
		// Once the outer ring reaches the terrain boundary there is no larger
		// ring to build.  Building another one would be an empty mesh (all of
		// its cells lie in the inner hole) and ModelMan correctly rejects it.
		if (level && extent <= previous_extent) {
			dynamic_models[level].reset();
			continue;
		}
		const float step=extent/(CLIPMAP_GRID-1), half=extent*.5f;
		ModelBuilder b; b.begin_submesh(master ? master : imaterials->get_fallback_sptr());
		std::array<std::array<uint16_t, CLIPMAP_GRID>, CLIPMAP_GRID> ids{};
		for (int z=0;z<CLIPMAP_GRID;z++) for(int x=0;x<CLIPMAP_GRID;x++) ids[z][x]=b.add_vertex({-half+x*step,0.f,-half+z*step},{0,0},{0,1,0});
		const float inner=level==0 ? -1.f : previous_extent*.5f;
		for(int z=0;z<CLIPMAP_GRID-1;z++) for(int x=0;x<CLIPMAP_GRID-1;x++) {
			const float cx=-half+(x+.5f)*step, cz=-half+(z+.5f)*step;
			if (level && std::abs(cx)<inner && std::abs(cz)<inner) continue;
			b.add_quad(ids[z][x],ids[z+1][x],ids[z+1][x+1],ids[z][x+1]);
		}
		if (!dynamic_models[level]) dynamic_models[level].reset(g_modelMgr.create_dynamic_model(b,"terrain_clipmap"));
		else g_modelMgr.refresh_dynamic_model(dynamic_models[level].get(),b);
	}
	clipmap_dirty=false;
}
void TerrainComponent::destroy_render_meshes() {
	for (int i=0;i<CLIPMAP_LEVELS;++i) { if(render_handles[i].is_valid()) idraw->get_scene()->remove_obj(render_handles[i]); dynamic_models[i].reset(); }
}
void TerrainComponent::update_clipmap() {
	if (!get_owner()) return;
	if (textures_dirty) update_terrain_material();
	if (clipmap_dirty) build_render_meshes();
	glm::vec3 eye(0); if (auto* camera=CameraComponent::get_scene_camera()) eye=camera->get_owner()->get_ws_position();
	for (int level=0;level<CLIPMAP_LEVELS;++level) {
		if (!dynamic_models[level]) continue;
		const float extent=glm::min(size,32.f*std::pow(2.f,(float)level));
		const float previous_extent=level ? glm::min(size,32.f*std::pow(2.f,(float)(level-1))) : 0.f;
		if (level && extent <= previous_extent) {
			if (render_handles[level].is_valid()) { Render_Object hidden; hidden.visible=false; idraw->get_scene()->update_obj(render_handles[level],hidden); }
			continue;
		}
		const float step=extent/(CLIPMAP_GRID-1);
		glm::vec2 centre(std::floor(eye.x/step)*step,std::floor(eye.z/step)*step);
		if (clipmap_centers[level]==centre && render_handles[level].is_valid()) continue;
		clipmap_centers[level]=centre;
		if(!render_handles[level].is_valid()) render_handles[level]=idraw->get_scene()->register_obj();
		Render_Object o; o.model=dynamic_models[level].get(); o.mat_override=terrain_material; o.transform=glm::translate(glm::mat4(1.f),glm::vec3(centre.x,0,centre.y)); o.visible=true; o.owner=this;
		idraw->get_scene()->update_obj(render_handles[level],o);
	}
}
void TerrainComponent::rebuild_runtime() { if(!get_owner()) return; ensure_gpu_textures(); update_terrain_material(); build_render_meshes(); rebuild_collision(); update_clipmap(); }
void TerrainComponent::on_sync_render_data() { update_clipmap(); }

void TerrainComponent::destroy_collision() { if(collision_actor) { g_physics.destroy_static_triangle_mesh(collision_actor); collision_actor=nullptr; } }
void TerrainComponent::rebuild_collision() {
	destroy_collision(); if(!collision_enabled) return; std::vector<glm::vec3> verts; std::vector<uint32_t> inds; verts.reserve((size_t)resolution*resolution);
	const float step=size/(resolution-1),half=size*.5f;
	for(int z=0;z<resolution;z++) for(int x=0;x<resolution;x++) verts.push_back({-half+x*step,height_at(x,z),-half+z*step});
	for(int z=0;z<resolution-1;z++) for(int x=0;x<resolution-1;x++){uint32_t a=index(x,z),b=index(x,z+1),c=index(x+1,z+1),d=index(x+1,z); inds.insert(inds.end(),{a,b,c,a,c,d});}
	collision_actor=g_physics.create_static_triangle_mesh(verts,inds,glm::mat4(1.f),PL::StaticObject);
}

bool TerrainComponent::save_data() { ensure_data(); TerrainFileHeader h; h.resolution=resolution; h.size=size; auto f=FileSys::open_write_game(data_path); if(!f) return false; f->write(&h,sizeof(h)); f->write(heights.data(),heights.size()*sizeof(float)); f->write(weights.data(),weights.size()); data_dirty=false; return true; }
bool TerrainComponent::load_data() {
	auto f=FileSys::open_read_game(data_path); if(!f) return false; TerrainFileHeader h{}; if(f->size()<sizeof(h)) return false; f->read(&h,sizeof(h));
	// The component owns its size/resolution.  A mismatched sidecar is stale data,
	// not permission to silently overwrite inspector values.
	if(memcmp(h.magic,"CSTERR1",7)||h.resolution!=(uint32_t)resolution) return false;
	ensure_data(); const size_t bytes=heights.size()*sizeof(float)+weights.size(); if(f->size()<sizeof(h)+bytes) return false;
	f->read(heights.data(),heights.size()*sizeof(float)); f->read(weights.data(),weights.size()); data_dirty=false; textures_dirty=true; return true;
}
void TerrainComponent::restore_edit_data(std::vector<float> h,std::vector<uint8_t> w) { heights=std::move(h); weights=std::move(w); ensure_data(); data_dirty=true; textures_dirty=true; update_terrain_material(); rebuild_collision(); }
void TerrainComponent::apply_brush(Brush brush,const glm::vec3& p,float radius,float strength,float falloff,int layer) {
	ensure_data(); const float step=size/(resolution-1); const int cx=(int)((p.x/size+.5f)*(resolution-1)), cz=(int)((p.z/size+.5f)*(resolution-1)), r=(int)ceil(radius/step);
	const float flatten_height=height_at(cx,cz);
	std::vector<float> source;
	if (brush==Brush::Smooth || brush==Brush::ThermalErosion) source=heights;
	for(int z=glm::max(0,cz-r);z<=glm::min(resolution-1,cz+r);++z) for(int x=glm::max(0,cx-r);x<=glm::min(resolution-1,cx+r);++x) {
		const float d=glm::length(glm::vec2((x-cx)*step,(z-cz)*step)); if(d>radius) continue; const float w=std::pow(glm::max(0.f,1.f-d/radius),glm::max(.01f,falloff)); const int i=index(x,z);
		if(brush==Brush::Raise) heights[i]+=strength*w; else if(brush==Brush::Lower) heights[i]-=strength*w;
		else if(brush==Brush::Flatten) heights[i]=glm::mix(heights[i],flatten_height,glm::clamp(strength*w,0.f,1.f));
		else if(brush==Brush::Noise) heights[i]+=sinf((float)(x*92821+z*68917))*strength*w;
		else if(brush==Brush::Smooth) { float avg=0.f; for(int oz=-1;oz<=1;++oz) for(int ox=-1;ox<=1;++ox) avg+=source[index(glm::clamp(x+ox,0,resolution-1),glm::clamp(z+oz,0,resolution-1))]; heights[i]=glm::mix(source[i],avg/9.f,glm::clamp(strength*w,0.f,1.f)); }
		else if(brush==Brush::ThermalErosion) { const float avg=(source[index(glm::max(0,x-1),z)]+source[index(glm::min(resolution-1,x+1),z)]+source[index(x,glm::max(0,z-1))]+source[index(x,glm::min(resolution-1,z+1))])*.25f; heights[i]=glm::mix(source[i],avg,glm::clamp(strength*w*.5f,0.f,1.f)); }
		else if(brush==Brush::PaintMaterial) { const int o=i*4, layer_i=glm::clamp(layer,0,3); weights[o+layer_i]=(uint8_t)glm::clamp((int)weights[o+layer_i]+(int)(strength*w*255),0,255); }
	}
	data_dirty=true; textures_dirty=true; update_terrain_material();
}
void TerrainComponent::finish_edit_stroke() { rebuild_collision(); }
