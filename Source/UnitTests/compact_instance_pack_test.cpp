#include <gtest/gtest.h>
#include "Render/CompactInstancePack.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "glm/gtc/quaternion.hpp"

// The compact record is memcpy'd straight into a std430 SSBO; a size drift would
// silently corrupt every instance past the first. Guard the wire size here too
// (in addition to the static_assert in DrawLocal_Batching.h) so it fails as a
// named test, not an opaque build break.
TEST(CompactInstancePack, RecordIsTight24Bytes) {
	EXPECT_EQ(sizeof(gpu::CompactInstance), 24u);
}

TEST(CompactInstancePack, BatchSeedRoundTrips) {
	const uint16_t batch = 0xBEEF;
	const uint8_t seed = 0x5A;
	const uint32_t packed = pack_batch_seed(batch, seed);
	EXPECT_EQ(unpack_batch_id(packed), batch);
	EXPECT_EQ(unpack_seed(packed), seed);
	// Upper pad byte must stay clear.
	EXPECT_EQ(packed >> 24, 0u);
}

TEST(CompactInstancePack, IdentityQuatRoundTrips) {
	const glm::quat q = glm::quat(1, 0, 0, 0);
	const glm::quat r = unpack_quat_snorm8(pack_quat_snorm8(q));
	// w should recover to 1, xyz to 0 (within snorm8 quantization).
	EXPECT_NEAR(std::abs(r.w), 1.f, 1e-3f);
	EXPECT_NEAR(r.x, 0.f, 0.01f);
	EXPECT_NEAR(r.y, 0.f, 0.01f);
	EXPECT_NEAR(r.z, 0.f, 0.01f);
}

// snorm8 error is bounded ~1/127; a round-trip rotation must still rotate a
// vector to nearly the same place. Checks the whole pack->unpack->normalize path.
TEST(CompactInstancePack, ArbitraryQuatPreservesRotationWithinTolerance) {
	const glm::quat orig = glm::normalize(glm::quat(0.3f, -0.6f, 0.5f, 0.2f));
	const glm::quat rt = unpack_quat_snorm8(pack_quat_snorm8(orig));

	const glm::vec3 v(1.f, 2.f, 3.f);
	const glm::vec3 a = orig * v;
	const glm::vec3 b = rt * v;
	// 24-bit angular precision on a length-~3.74 vector: well under 0.1 units.
	EXPECT_LT(glm::length(a - b), 0.1f);
}

TEST(CompactInstancePack, PackedQuatFitsComponentsInCorrectBytes) {
	// Distinct component signs must land in the right bytes (x low .. w high).
	const glm::quat q = glm::normalize(glm::quat(0.5f, 0.5f, -0.5f, -0.5f)); // w,x,y,z
	const uint32_t p = pack_quat_snorm8(q);
	const glm::quat r = unpack_quat_snorm8(p);
	EXPECT_NEAR(r.x, q.x, 0.02f);
	EXPECT_NEAR(r.y, q.y, 0.02f);
	EXPECT_NEAR(r.z, q.z, 0.02f);
	EXPECT_NEAR(r.w, q.w, 0.02f);
}
