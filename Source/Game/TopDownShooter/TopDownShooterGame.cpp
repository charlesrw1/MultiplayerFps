#include "TopDownShooterGame.h"

//afad


TopDownGameManager* TopDownGameManager::instance = nullptr;

#include "Game/Components/LightComponents.h"

float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float lerp(float t, float a, float b) {
    return a + t * (b - a);
}
void randomGradient(int ix, int iy, float& gx, float& gy) {
    float random = 2920.0f * sin(ix * 21942.0f + iy * 171324.0f + 8912.0f) * cos(ix * 23157.0f * iy * 217832.0f + 9758.0f);
    gx = cos(random);
    gy = sin(random);
}
float dotGridGradient(int ix, int iy, float x, float y) {
    float gx, gy;
	randomGradient(ix, iy, gx, gy);
    float dx = x - static_cast<float>(ix);
    float dy = y - static_cast<float>(iy);
    return (dx * gx + dy * gy);
}
float perlin(float x, float y) {
    int x0 = static_cast<int>(floor(x));
    int x1 = x0 + 1;
    int y0 = static_cast<int>(floor(y));
    int y1 = y0 + 1;

    float sx = fade(x - static_cast<float>(x0));
    float sy = fade(y - static_cast<float>(y0));

    float n0, n1, ix0, ix1, value;
    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = lerp(sx, n0, n1);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = lerp(sx, n0, n1);

    value = lerp(sy, ix0, ix1);
    return (value + 1.0) / 2.0;  // Normalize to [0, 1]
}


CLASS_H(TopDownFireScript,EntityComponent)
public:
	TopDownFireScript() {
		set_call_init_in_editor(true);
	}
	void start() override {
		light = get_owner()->get_first_component<PointLightComponent>();
		set_ticking(true);
	}
	void update() override {
		if (!light) return;
		float a = perlin(eng->get_game_time() * 2.0+ofs, eng->get_game_time()-ofs*0.2);
		light->intensity = glm::mix(min_intensity,max_intensity,pow(a,2.0));
		light->on_changed_transform();
	}

	float ofs = 0.0;
	float min_intensity = 0.0;
	float max_intensity = 30.0;
	PointLightComponent* light = nullptr;
};
CLASS_IMPL(TopDownFireScript);

