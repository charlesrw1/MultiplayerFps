#pragma once
#include "Particles/ParticlesPublic.h"
#include "Framework/ClassBase.h"
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include "Framework/MathLib.h"
#include "Assets/IAsset.h"

#include "Assets/AssetRegistry.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/FreeList.h"
#include "Framework/ObjectSerialization.h"
#include "Framework/Files.h"
#include "Framework/MeshBuilder.h"
enum class ScalarFields
{
	Lifetime,
	Rotation,
	RotationRate,
	Scale,
	PositionX,
	PositionY,
	PositionZ,
	VelocityX,
	VelocityY,
	VelocityZ,

	ColorR,
	ColorG,
	ColorB,
	Alpha,
};
enum class VectorFields
{
	Position,
	Velocity,
};

struct ParticleDef
{
	glm::vec3 position{};
	glm::vec3 velocity{};
	float lifeTime = 0.0;
	float totalLife = 0.0;
	float scale{};
	float rotation{};
	float rotationRate{};
	Color32 colorA{};
	Color32 colorB{};
	uint8_t lightIndex{};

	float* get_scalar_f(ScalarFields f) {
		switch (f) {
		case ScalarFields::Lifetime: return &lifeTime;
		case ScalarFields::Rotation: return &rotation;
		case ScalarFields::RotationRate: return &rotationRate;
		case ScalarFields::Scale: return &scale;
		case ScalarFields::PositionX: return &position.x;
		case ScalarFields::PositionY: return &position.y;
		case ScalarFields::PositionZ: return &position.z;
		case ScalarFields::VelocityX: return &velocity.x;
		case ScalarFields::VelocityY: return &velocity.y;
		case ScalarFields::VelocityZ: return &velocity.z;
		}
		return nullptr;
	}
	uint8_t* get_scalar_u8(ScalarFields f) {
		switch (f) {
		case ScalarFields::ColorR: return &colorA.r;
		case ScalarFields::ColorG: return&colorA.g;
		case ScalarFields::ColorB: return &colorA.b;
		case ScalarFields::Alpha: return &colorA.a;
		}
		return nullptr;
	}
};
struct ParticleVertex
{
	glm::vec3 position;
	uint8_t color[4];
	uint8_t texCoord[2];
	uint8_t extra[2];
};

class ParticleSystemDef;
class ParticleEmitterInst
{
public:
	std::vector<ParticleDef> allParticles;
	int creationStart = 0;
};
class ParticleSystemInst
{
public:
	ParticleSystemInst() : randomGen(425235247) {}
	glm::vec3 position{};	// system position
	glm::vec3 velocity{};
	glm::quat rotation{};	// system rotation
	float systemTime = 0.0;
	ParticleSystemDef* systemAsset = nullptr;
	Random randomGen;
	std::vector<ParticleEmitterInst> emitterInsts;

	void update(float dt);
};


// A piece of logic that operates on particles
CLASS_H(ParticleModule,ClassBase)
public:
	virtual void update(ParticleSystemInst* inst, ParticleEmitterInst* emit, float dt) const = 0;
};

CLASS_H(P_EmitBurst, ParticleModule)
public:
	void update(ParticleSystemInst* inst, ParticleEmitterInst* emitter, float dt) const {
		if (inst->systemTime > emit_time && inst->systemTime <= emit_time - dt) {
			int count = glm::pow(inst->randomGen.RandI(emit_min, emit_max), emite_exp);
			emitter->allParticles.resize(emitter->allParticles.size() + count);
		}
	}

	float emit_time = 0.0;
	int emit_min = 0;
	int emit_max = 1;
	float emite_exp = 1.0;
};
CLASS_H(P_EmitContinously, ParticleModule)
public:
	void update(ParticleSystemInst* inst, ParticleEmitterInst* emitter, float dt) const {
		const float next_emit_time = glm::ceil(inst->systemTime / rate) * rate;
		int numParticlesToEmit = static_cast<int>(std::floor(dt * rate));
		if (next_emit_time < inst->systemTime && next_emit_time >= inst->systemTime - dt)
			numParticlesToEmit += 1;
		emitter->allParticles.resize(emitter->allParticles.size() + numParticlesToEmit);
	}
	float start = 0.0;
	float emit_time = 10000.0;
	float rate = 1.0;
};
CLASS_H(P_InitPositionInSphere, ParticleModule)
public:
};
CLASS_H(P_InitScalarRandom, ParticleModule)
public:
	float minVal{};
	float maxVal{};
	float exp = 1.0;
	ScalarFields field{};
};
CLASS_H(P_InitVelocityRandom, ParticleModule)
public:
	float speedMin = 0.0;
	float speedMax = 0.0;
	float speedExp = 1.0;
	glm::vec3 offset{};
	bool normalize = false;
};
CLASS_H(P_BasicMovement,ParticleModule)
public:
	void update(ParticleSystemInst* inst, ParticleEmitterInst* emitter, float dt) const {
		for (int i = 0; i < emitter->creationStart; i++) {
			auto& p = emitter->allParticles[i];
			p.velocity.y -= gravity * 9.8;
			p.velocity -= p.velocity * decay * dt;
		}
	}
	float gravity = 1.0;
	float decay = 0.1;	// velocity drag
	float angularDrag = 1.0;
};
CLASS_H(P_SpawnLight, ParticleModule)
public:
};
CLASS_H(P_DecayScalar, ParticleModule)
public:
	void update(ParticleSystemInst* inst, ParticleEmitterInst* emitter, float dt) const {
		if(field < ScalarFields::ColorR) {
			for (int i = 0; i < emitter->creationStart; i++) {
				auto& p = emitter->allParticles[i];
				float* ptr = p.get_scalar_f(field);
				*ptr -= *ptr * decay * dt;
			}
		}
	}
	float decay = 0.5;
	ScalarFields field{};
};

CLASS_H(P_ScalarOverLife, ParticleModule)
public:
	float minVel{};
	float maxVel{};
	float exp = 1.0;
	ScalarFields field{};
};
CLASS_H(P_CopyScalarField,ParticleModule)
public:
	ScalarFields src{};
	ScalarFields dest{};
	bool remap = false;
	float srcMin{};
	float srcMax{};
	float destMin{};
	float destMax{};
	bool clamp = false;
	float min{};
	float max{};
};
CLASS_H(P_OscillateScalar,ParticleModule)
public:
	bool additive = false;
	bool useGlobalTime = true;
	ScalarFields field{};
};
CLASS_H(P_AttractorForce, ParticleModule)
public:
	void update(ParticleSystemInst* inst, ParticleEmitterInst* emitter, float dt) const {
		for (int i = 0; i < emitter->creationStart; i++) {
			auto& p = emitter->allParticles[i];
			p.velocity -= p.position * force * dt;
		}
	}
	bool applyAsCylinder = false;	// twists around axis if true, else just pulls to center of system
	glm::vec3 twistAxis = glm::vec3(0, 1, 0);
	float force = 1.0;
};
#include "Framework/ArrayReflection.h"

template<>
struct GetAtomValueWrapper<std::unique_ptr<ParticleModule>> {
	static PropertyInfo get() {
		PropertyInfo pi;
		pi.offset = 0;
		pi.name = "_value";
		pi.type = core_type_id::StdUniquePtr;
		pi.flags = PROP_DEFAULT;
		return pi;
	}
};
template<>
struct GetAtomValueWrapper<std::unique_ptr<ParticleEmitter>> {
	static PropertyInfo get() {
		PropertyInfo pi;
		pi.offset = 0;
		pi.name = "_value";
		pi.type = core_type_id::StdUniquePtr;
		pi.flags = PROP_DEFAULT;
		return pi;
	}
};
#include "Framework/ReflectionMacros.h"
class Model;
class MaterialInstance;
CLASS_H(ParticleEmitter, ClassBase)
public:
	ParticleEmitter();
	~ParticleEmitter();
	std::vector<std::unique_ptr<ParticleModule>> module_stack;
	// what material, rendering type (models vs sprites)
	AssetPtr<MaterialInstance> spriteMaterial;
	AssetPtr<Model> particleModel;

	static const PropertyInfoList* get_props();
};


struct ParticleFxInternal
{
	ParticleFxInternal() {}
	~ParticleFxInternal() {
		assert(instance == nullptr);
	}

	ParticleFx fx;
	float lastUpdateTime = 0.0;
	glm::vec3 veloctyEst{};
	ParticleSystemInst* instance = nullptr;
};
class ParticleSystemLocal : public ParticleSystemPublic
{
public:
	// public interface

	void simulate_all_systems() {}	// update systems
	void init() {}
	handle<ParticleFx> create_fx(const ParticleFx& fx) {
		return { -1 };
	}
	void update_fx(handle<ParticleEmitter> emitter, const ParticleFx& fx) {}
	void free_fx(handle<ParticleEmitter>& handle) {}


	ParticleFXAsset* load_particle_fx(const std::string& file) {


		std::string path = file;

		auto filePtr = FileSys::open_read_game(path.c_str());
		if (!filePtr) {
			sys_print("!!! couldnt load particle fx file %s\n", file.c_str());
			return nullptr;
		}

		DictParser dp;
		dp.load_from_file(filePtr.get());
		StringView tok;
		dp.read_string(tok);
		auto newAsset = read_object_properties<ParticleFXAsset>(nullptr, dp, tok);

		if (!newAsset) {
			sys_print("!!! couldnt load particle fx %s\n", file.c_str());
			return nullptr;
		}


		return newAsset;
	}
public:
	// local interface (used by local renderer)
	void prep_particle_buffers();	// upload the data
	void scene_draw();	// draw the data

	Free_List<ParticleFxInternal> all_systems;

	// dynamic buffers
	MeshBuilder meshBuilder;


};