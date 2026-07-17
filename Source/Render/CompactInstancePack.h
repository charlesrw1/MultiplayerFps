#pragma once
// Bit-packing helpers for the compact instance record (gpu::CompactInstance).
// These define the CPU<->GPU wire format for the packed rotation and the
// batch_id/seed word; the shader-side unpack (CullComputeCompact / the
// COMPACT_INST master permutation) must mirror pack_quat_snorm8 /
// pack_batch_seed exactly. Kept dependency-free (glm only) so it is unit
// testable without a graphics context.

#include "glm/gtc/quaternion.hpp"
#include "glm/common.hpp"
#include <cstdint>

// --- rotation: unit quaternion as 4x snorm8 --------------------------------
// Each component c in [-1,1] -> round(c*127) stored as a signed byte, x in the
// low byte. snorm8 gives ~0.008 rad worst-case error, plenty for foliage/debris
// where the compact path is used. Not smallest-three: 4x8 is branchless on both
// sides and the extra byte buys nothing back here.
inline uint32_t pack_quat_snorm8(const glm::quat& q_in) {
	glm::quat q = glm::normalize(q_in);
	auto to_byte = [](float c) -> uint32_t {
		const float clamped = glm::clamp(c, -1.f, 1.f);
		const int32_t i = (int32_t)glm::round(clamped * 127.f);
		return (uint32_t)(uint8_t)(int8_t)i;
	};
	return to_byte(q.x) | (to_byte(q.y) << 8) | (to_byte(q.z) << 16) | (to_byte(q.w) << 24);
}

inline glm::quat unpack_quat_snorm8(uint32_t packed) {
	auto from_byte = [](uint32_t bits) -> float {
		const int8_t s = (int8_t)(uint8_t)(bits & 0xFF);
		return glm::clamp((float)s / 127.f, -1.f, 1.f);
	};
	glm::quat q;
	q.x = from_byte(packed);
	q.y = from_byte(packed >> 8);
	q.z = from_byte(packed >> 16);
	q.w = from_byte(packed >> 24);
	return glm::normalize(q);
}

// --- batch_id (16 bits) + seed (8 bits) packed into one uint ----------------
inline uint32_t pack_batch_seed(uint16_t batch_id, uint8_t seed) {
	return (uint32_t)batch_id | ((uint32_t)seed << 16);
}
inline uint16_t unpack_batch_id(uint32_t packed) { return (uint16_t)(packed & 0xFFFF); }
inline uint8_t unpack_seed(uint32_t packed) { return (uint8_t)((packed >> 16) & 0xFF); }
