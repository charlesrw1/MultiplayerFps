#pragma once
#include <string>
#include "Game/Entity.h"
#include <memory>
#include "Game/Components/MeshComponent.h"
#include <json.hpp>
template<typename T>
using wptr = std::weak_ptr<T>;
template<typename T>
using sptr = std::shared_ptr<T>;

class EditorObject : public ClassBase {
public:
	CLASS_BODY(EditorObject);
	REF std::string editorName;
	bool hidden = false;
	bool selectable = false;
	bool selected = false;
	bool locked = false;

	bool dont_serialize = false;
	bool dont_show = false;
	bool cant_delete = false;

	EntityPtr entity;

	glm::vec3 euler_angles{};

	virtual void unserialize(const nlohmann::json& json) {}
	virtual void serialize(nlohmann::json& json) {}
	virtual void imgui_draw(bool first_frame) {}
};
class EditorDoc;
class LevelArchive;


#include "Render/Model.h"
// object types

// map settings: skylight/lightmap

// static mesh, with physics options
// 95% of level is this
class ModelEditorObject : public EditorObject {
public:
	void serialize(nlohmann::json& json) {
		
	}
	void imgui_draw() {
	}

	glm::vec4 lm_coords{};
	bool probe_lit = false;
	bool physics_enabled = true;
	bool cast_shadows = false;
	bool is_skybox = false;
	Model* model = nullptr;
};


// spot/sun/point
class LightEditorObject
{
public:
};
// decal
class DecalEditorObject
{
public:
};
// physics area
class AreaEditorObject
{
public:
};
class CubemapEditorObject
{
public:
};
class ParticleEditorObject
{
public:
};
class SoundEditorObject
{
public:
};
class SpawnerObject
{
public:
};
