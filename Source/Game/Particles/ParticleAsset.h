#pragma once
#include "Assets/IAsset.h"
#include "ParticleModules.h"
#include <vector>
#include <string>

struct ParticleSubSystem
{
	std::string name = "SubSystem";
	bool editor_visible = true;

	MainModule main;
	EmissionModule emission;
	ShapeModule shape;
	VelocityOverLifetimeModule velocity_over_lifetime;
	ColorOverLifetimeModule color_over_lifetime;
	SizeOverLifetimeModule size_over_lifetime;
	RotationOverLifetimeModule rotation_over_lifetime;
	TextureSheetModule texture_sheet;
	NoiseModule noise;
	RendererModule renderer;
};

void to_json(nlohmann::json& j, const ParticleSubSystem& ss);
void from_json(const nlohmann::json& j, ParticleSubSystem& ss);

class ParticleAsset : public IAsset
{
public:
	CLASS_BODY(ParticleAsset);

	bool load_asset() final;
	void post_load() final;
	void uninstall() final;

	void save_to_disk();

	std::vector<ParticleSubSystem> subsystems;
};
