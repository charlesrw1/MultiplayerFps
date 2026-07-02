#include "ParticleAsset.h"
#include "Framework/Files.h"
#include "Assets/AssetDatabase.h"

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"

class ParticleAssetMetadata : public AssetMetadata
{
public:
	ParticleAssetMetadata() { extensions.push_back("particle"); }
	Color32 get_browser_color() const override { return {255, 140, 30}; }
	std::string get_type_name() const override { return "Particle"; }
	const ClassTypeInfo* get_asset_class_type() const override { return &ParticleAsset::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(ParticleAssetMetadata);
#endif

void to_json(nlohmann::json& j, const ParticleSubSystem& ss)
{
	j["name"] = ss.name;
	j["editor_visible"] = ss.editor_visible;
	j["main"] = ss.main;
	j["emission"] = ss.emission;
	j["shape"] = ss.shape;
	j["velocity_over_lifetime"] = ss.velocity_over_lifetime;
	j["force_over_lifetime"] = ss.force_over_lifetime;
	j["limit_velocity_over_lifetime"] = ss.limit_velocity_over_lifetime;
	j["color_over_lifetime"] = ss.color_over_lifetime;
	j["size_over_lifetime"] = ss.size_over_lifetime;
	j["rotation_over_lifetime"] = ss.rotation_over_lifetime;
	j["texture_sheet"] = ss.texture_sheet;
	j["noise"] = ss.noise;
	j["trail"] = ss.trail;
	j["renderer"] = ss.renderer;
	j["lifetime_by_emitter_speed"] = ss.lifetime_by_emitter_speed;
	j["inherit_velocity"] = ss.inherit_velocity;
}

void from_json(const nlohmann::json& j, ParticleSubSystem& ss)
{
	ss.name = j.value("name", std::string("SubSystem"));
	ss.editor_visible = j.value("editor_visible", true);
	if (j.contains("main")) j["main"].get_to(ss.main);
	if (j.contains("emission")) j["emission"].get_to(ss.emission);
	if (j.contains("shape")) j["shape"].get_to(ss.shape);
	if (j.contains("velocity_over_lifetime")) j["velocity_over_lifetime"].get_to(ss.velocity_over_lifetime);
	if (j.contains("force_over_lifetime")) j["force_over_lifetime"].get_to(ss.force_over_lifetime);
	if (j.contains("limit_velocity_over_lifetime")) j["limit_velocity_over_lifetime"].get_to(ss.limit_velocity_over_lifetime);
	if (j.contains("color_over_lifetime")) j["color_over_lifetime"].get_to(ss.color_over_lifetime);
	if (j.contains("size_over_lifetime")) j["size_over_lifetime"].get_to(ss.size_over_lifetime);
	if (j.contains("rotation_over_lifetime")) j["rotation_over_lifetime"].get_to(ss.rotation_over_lifetime);
	if (j.contains("texture_sheet")) j["texture_sheet"].get_to(ss.texture_sheet);
	if (j.contains("noise")) j["noise"].get_to(ss.noise);
	if (j.contains("trail")) j["trail"].get_to(ss.trail);
	if (j.contains("renderer")) j["renderer"].get_to(ss.renderer);
	if (j.contains("lifetime_by_emitter_speed")) j["lifetime_by_emitter_speed"].get_to(ss.lifetime_by_emitter_speed);
	if (j.contains("inherit_velocity")) j["inherit_velocity"].get_to(ss.inherit_velocity);
}

bool ParticleAsset::load_asset()
{
	auto file = FileSys::open_read_game(get_name());
	if (!file) {
		sys_print(Warning, "ParticleAsset: failed to open %s\n", get_name().c_str());
		return false;
	}

	size_t file_size = file->size();
	std::string text;
	text.resize(file_size);
	file->read(text.data(), file_size);
	file->close();

	try {
		auto root = nlohmann::json::parse(text);
		subsystems.clear();
		if (root.contains("subsystems")) {
			for (auto& ssj : root["subsystems"]) {
				ParticleSubSystem ss;
				from_json(ssj, ss);
				subsystems.push_back(std::move(ss));
			}
		}
	}
	catch (const nlohmann::json::exception& e) {
		sys_print(Warning, "ParticleAsset: JSON parse error in %s: %s\n", get_name().c_str(), e.what());
		return false;
	}

	return true;
}

void ParticleAsset::post_load()
{
}

void ParticleAsset::uninstall()
{
	subsystems.clear();
}

void ParticleAsset::save_to_disk()
{
	sys_print(Info, "ParticleAsset::save_to_disk: this=%p path=%s subsystems=%zu\n",
		(void*)this, get_name().c_str(), subsystems.size());

	nlohmann::json root;
	auto& arr = root["subsystems"] = nlohmann::json::array();
	for (auto& ss : subsystems) {
		nlohmann::json ssj;
		to_json(ssj, ss);
		arr.push_back(ssj);
	}

	std::string text = root.dump(2);
	auto file = FileSys::open_write_game(get_name());
	if (!file) {
		sys_print(Warning, "ParticleAsset: failed to write %s\n", get_name().c_str());
		return;
	}
	file->write(text.data(), text.size());
	file->close();

	sys_print(Info, "ParticleAsset::save_to_disk: wrote %zu bytes to %s\n", text.size(), get_name().c_str());
}
