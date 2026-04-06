#include "CharacterController.h"
#include "BikeEntity.h"
#include "Game/Components/MeshComponent.h"
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"
#include "Game/Entity.h"

#include "Framework/MathLib.h"

#include "GameEnginePublic.h"


static glm::mat4 last_transform{};
void BikeCppUtils::debug_pre_draw_bike(Entity* bike) {
	last_transform = bike->get_ws_transform();
}

void BikeCppUtils::debug_draw_bike(Entity* bike, float wheel_ofs0, float wheel_ofs1, float lifetime) {
	auto draw = [&](float ofs, Color32 color) {
		glm::vec3 p00 = last_transform * glm::vec4(0, 0, ofs, 1);
		glm::vec3 p01 = bike->get_ws_transform() * glm::vec4(0, 0, ofs, 1);
		//Debug::add_line(p00, p01, color, lifetime);
		//Debug::add_line(p00, p01, color, lifetime);
	};
	draw(wheel_ofs0, Color32{200, 20, 20});
	draw(wheel_ofs1, Color32{20, 20, 200});
}
