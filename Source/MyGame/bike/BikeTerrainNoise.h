// BikeTerrainNoise.h
// Self-contained 2D gradient (Perlin) noise + fractal-octave sum, used by the
// "Hilly" hardcoded course (BikeCourseHilly.cpp) to generate both the terrain
// mesh heightfield and the road's elevation profile from the same function.
#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>

namespace bike_noise {

// Classic Perlin permutation table, hashed from a seed so different seeds
// produce different (but still deterministic/repeatable) terrain.
class Perlin2D {
public:
	explicit Perlin2D(uint32_t seed = 1337) { reseed(seed); }

	void reseed(uint32_t seed) {
		for (int i = 0; i < 256; ++i) perm[i] = (uint8_t)i;
		// Simple xorshift-based Fisher-Yates shuffle -- deterministic per seed,
		// no dependency on <random>'s engine-specific sequences.
		uint32_t s = seed ? seed : 1;
		for (int i = 255; i > 0; --i) {
			s ^= s << 13; s ^= s >> 17; s ^= s << 5;
			const int j = (int)(s % (uint32_t)(i + 1));
			std::swap(perm[i], perm[j]);
		}
		for (int i = 0; i < 256; ++i) perm[256 + i] = perm[i];
	}

	// Single-octave gradient noise, range approximately [-1, 1].
	float noise(float x, float y) const {
		const int xi = (int)std::floor(x) & 255;
		const int yi = (int)std::floor(y) & 255;
		const float xf = x - std::floor(x);
		const float yf = y - std::floor(y);
		const float u = fade(xf);
		const float v = fade(yf);

		const int aa = perm[perm[xi] + yi];
		const int ab = perm[perm[xi] + yi + 1];
		const int ba = perm[perm[xi + 1] + yi];
		const int bb = perm[perm[xi + 1] + yi + 1];

		const float x1 = lerp(grad(aa, xf, yf),       grad(ba, xf - 1.f, yf),       u);
		const float x2 = lerp(grad(ab, xf, yf - 1.f), grad(bb, xf - 1.f, yf - 1.f), u);
		return lerp(x1, x2, v);
	}

	// Sum of octaves (fractional Brownian motion). Each octave doubles
	// frequency (lacunarity) and scales amplitude down by gain, matching the
	// standard fBm formulation. Result is normalized back into ~[-1, 1] by
	// dividing by the sum of amplitudes actually used.
	float fbm(float x, float y, int octaves, float base_freq, float lacunarity, float gain) const {
		float sum = 0.f, amp = 1.f, freq = base_freq, amp_total = 0.f;
		for (int i = 0; i < octaves; ++i) {
			sum += noise(x * freq, y * freq) * amp;
			amp_total += amp;
			amp  *= gain;
			freq *= lacunarity;
		}
		return amp_total > 1e-6f ? sum / amp_total : 0.f;
	}

private:
	uint8_t perm[512];

	static float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
	static float lerp(float a, float b, float t) { return a + t * (b - a); }
	static float grad(int hash, float x, float y) {
		// 8 gradient directions is enough for terrain-quality noise.
		switch (hash & 7) {
			case 0: return  x + y;
			case 1: return  x - y;
			case 2: return -x + y;
			case 3: return -x - y;
			case 4: return  x;
			case 5: return -x;
			case 6: return  y;
			default: return -y;
		}
	}
};

} // namespace bike_noise
