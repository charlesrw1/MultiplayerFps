#include "Types.h"
#include "Level.h"
#include "GameEngineLocal.h"
#include "Render/DrawPublic.h"
#include "Physics/Physics2.h"



static float mid_lerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}
float modulo_lerp(float start, float end, float mod, float alpha)
{
	float d1 = glm::abs(end - start);
	float d2 = mod - d1;


	if (d1 <= d2)
		return glm::mix(start, end, alpha);
	else {
		if (start >= end)
			return fmod(start + (alpha * d2), mod);
		else
			return fmod(end + ((1 - alpha) * d2), mod);
	}
}

#if 0
void RenderInterpolationComponent::evaluate(float time)
{
	int entry_index = 0;
	for (; entry_index < interpdata.size(); entry_index++) {
		auto& e = get(interpdata_head - 1 - entry_index);
		if (time >= e.time)
			break;
	}

	if (entry_index == interpdata.size()) {
		printf("all entries greater than time\n");
		ASSERT(get(interpdata_head - 1).time >= 0.f);
		lerped_pos = get(interpdata_head - 1).position;
		lerped_rot = get(interpdata_head - 1).rotation;
		return;
	}
	if (entry_index == 0) {
		printf("all entries less than time\n");
		float diff = time - get(interpdata_head - 1).time;
		ASSERT(diff >= 0.f);
		if (diff > max_extrapolate_time) {
			lerped_pos = get(interpdata_head - 1).position;
			lerped_rot = get(interpdata_head - 1).rotation;
		}
		else {
			printf("extrapolate\n");
			auto& a = get(interpdata_head - 2);
			auto& b = get(interpdata_head - 1);
			float extrap = mid_lerp(a.time, b.time, time);
			lerped_pos = glm::mix(a.position, b.position, extrap);
			for (int i = 0; i < 3; i++)
				lerped_rot[i] = modulo_lerp(a.rotation[i], b.rotation[i], TWOPI, extrap);
		}

		return;
	}

	auto& a = get(interpdata_head - 1 - entry_index);
	auto& b = get(interpdata_head - entry_index);

	if (a.time < 0 && b.time < 0) {
		printf("no state\n");
		lerped_pos = glm::vec3(0.f);
		lerped_rot = glm::vec3(0.f);
	}
	else if (a.time < 0) {
		printf("only one state\n");
		lerped_pos = b.position;
		lerped_rot = b.rotation;
	}
	else if (b.time < 0) {
		ASSERT(0);
	}
	else {
		ASSERT(a.time < b.time);
		ASSERT(a.time <= time && b.time >= time);

		float dist = glm::length(b.position - a.position);
		float speed = dist / (b.time - a.time);

		if (speed > telport_velocity_threshold) {
			lerped_pos = b.position;
			lerped_rot = b.rotation;
			sys_print("teleport\n");
		}
		else {
			float midlerp = mid_lerp(a.time, b.time, time);
			lerped_pos = glm::mix(a.position, b.position, midlerp);
			for (int i = 0; i < 3; i++)
				lerped_rot[i] = modulo_lerp(a.rotation[i], b.rotation[i], TWOPI, midlerp);

		}
	}

}

void RenderInterpolationComponent::add_state(float time, vec3 p, vec3 r)
{
	auto& element = get(interpdata_head);
	element.time = time;
	element.position = p;
	element.rotation = r;
	interpdata_head = (interpdata_head + 1) % interpdata.size();
}

void RenderInterpolationComponent::clear()
{
	for (int i = 0; i < interpdata.size(); i++) interpdata[i].time = -1.f;
}
#endif

#include <PxPhysics.h>



#include "Framework/MeshBuilder.h"
#include "Framework/Config.h"


