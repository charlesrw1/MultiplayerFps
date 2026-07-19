#pragma once
#include "DrawLocal.h"

extern ConfigVar r_taa_samples;
extern ConfigVar r_taa_32f;

class TaaManager
{
public:
	static const int MAX_TAA_SAMPLES = 16;	
	TaaManager() { generateHaltonSequence(MAX_TAA_SAMPLES, jitters); }

	void start_frame() { index = (index + 1) % r_taa_samples.get_integer(); }
	int get_frame_index() const { return index; }
	glm::vec2 get_last_frame_jitter(int w, int h) const {
		int previndex = index - 1;
		if (previndex < 0)
			previndex = r_taa_samples.get_integer() - 1;
		return calc_jitter(previndex, w, h);
	}
	glm::vec2 calc_frame_jitter(int width, int height) const { return calc_jitter(index, width, height); }
	glm::mat4 add_jitter_to_projection(const glm::mat4& inproj, glm::vec2 jitter) const {
		glm::mat4 matrix = inproj;
		matrix[2][0] += jitter.x;
		matrix[2][1] += jitter.y;

		return matrix;
	}

private:
	glm::vec2 calc_jitter(int the_index, int width, int height) const {
		ASSERT(width > 0 && height > 0);
		auto jit = jitters[the_index]; // [0,1]
		jit = jit - glm::vec2(0.5);	   //[-1/2,1/2]
		return glm::vec2(jit.x / width, jit.y / height);
	}

	static float radicalInverse(int base, int index) {
		ASSERT(base >= 2);
		float result = 0.0;
		float fraction = 1.0 / base;
		while (index > 0) {
			result += (index % base) * fraction;
			index /= base;
			fraction /= base;
		}
		return result;
	}
	static void generateHaltonSequence(int numPoints, glm::vec2* sequence) {
		ASSERT(numPoints > 0 && sequence != nullptr);
		const int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};
		const int dimension = 2;
		for (int d = 0; d < dimension; ++d) {
			int base = primes[d];
			for (int i = 0; i < numPoints; ++i) {
				sequence[i][d] = radicalInverse(base, i + 1);
			}
		}
	}

	glm::vec2 jitters[MAX_TAA_SAMPLES];
	int index = 0;
};
extern TaaManager r_taa_manager;