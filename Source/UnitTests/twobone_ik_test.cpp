#include <gtest/gtest.h>
#include "Animation/AnimationUtil.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Two-bone IK: joints a (root) -> b (mid/elbow) -> c (end/wrist). util_twobone_ik must
// place c on the target whenever the target is within reach, INCLUDING when the target is
// out of the current a-b-c plane (the case that regressed the FPS hand IK: reach came out
// correct but the hand landed off to the side).

namespace {
struct Bone { glm::quat q; glm::vec3 p; };

glm::mat4 local_mat(const Bone& b) {
	glm::mat4 m = glm::mat4_cast(b.q);
	m[3] = glm::vec4(b.p, 1.f);
	return m;
}

// Runs the solver on chain A(root)->B->C and returns the final root pos `a` and end pos `c`.
void solve(Bone A, Bone B, Bone C, glm::vec3 target, glm::vec3& out_a, glm::vec3& out_c) {
	glm::mat4 Ga = local_mat(A);
	glm::mat4 Gb = Ga * local_mat(B);
	glm::mat4 Gc = Gb * local_mat(C);
	const glm::vec3 a = Ga[3], b = Gb[3], c = Gc[3];
	const glm::quat a_gr = glm::quat_cast(Ga);
	const glm::quat b_gr = glm::quat_cast(Gb);

	util_twobone_ik(a, b, c, target, glm::vec3(0, 0, 1), a_gr, b_gr, A.q, B.q);

	// forward kinematics with the updated local rotations
	Ga = local_mat(A);
	Gb = Ga * local_mat(B);
	Gc = Gb * local_mat(C);
	out_a = Ga[3];
	out_c = Gc[3];
}

// Final |c - target| error.
float solve_error(Bone A, Bone B, Bone C, glm::vec3 target) {
	glm::vec3 a, c;
	solve(A, B, C, target, a, c);
	return glm::length(c - target);
}

// Same as solve(), but with an explicit pole target and returning the elbow (b) position too.
void solve_with_pole(Bone A, Bone B, Bone C, glm::vec3 target, glm::vec3 pole,
					  glm::vec3& out_a, glm::vec3& out_b, glm::vec3& out_c) {
	glm::mat4 Ga = local_mat(A);
	glm::mat4 Gb = Ga * local_mat(B);
	glm::mat4 Gc = Gb * local_mat(C);
	const glm::vec3 a = Ga[3], b = Gb[3], c = Gc[3];
	const glm::quat a_gr = glm::quat_cast(Ga);
	const glm::quat b_gr = glm::quat_cast(Gb);

	util_twobone_ik(a, b, c, target, pole, a_gr, b_gr, A.q, B.q);

	Ga = local_mat(A);
	Gb = Ga * local_mat(B);
	Gc = Gb * local_mat(C);
	out_a = Ga[3];
	out_b = Gb[3];
	out_c = Gc[3];
}

constexpr float kEps = 1e-3f; // 1 mm
} // namespace

TEST(TwoBoneIkTest, PlanarReachable) {
	Bone A{ glm::quat(1,0,0,0), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	EXPECT_LT(solve_error(A, B, C, glm::vec3(1.4f, 0.2f, 0.f)), kEps);
}

TEST(TwoBoneIkTest, PlanarOtherSide) {
	Bone A{ glm::quat(1,0,0,0), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	EXPECT_LT(solve_error(A, B, C, glm::vec3(0.5f, 1.2f, 0.f)), kEps);
}

// The regression: target out of the a-b-c plane. Before the aim-first fix this left the
// hand ~0.09 off target even though |a-c| matched |a-target|.
TEST(TwoBoneIkTest, OutOfPlaneTarget) {
	Bone A{ glm::angleAxis(glm::radians(30.f), glm::normalize(glm::vec3(0,1,1))), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(70.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	EXPECT_LT(solve_error(A, B, C, glm::vec3(1.0f, 0.6f, 0.5f)), kEps);
}

// Arm-like proportions and an out-of-plane target near the shoulder (mirrors the FPS case).
TEST(TwoBoneIkTest, ArmProportionsOutOfPlane) {
	Bone A{ glm::angleAxis(glm::radians(-50.f), glm::normalize(glm::vec3(1,0.3f,0.7f))), glm::vec3(0.2f,1.f,0.1f) };
	Bone B{ glm::angleAxis(glm::radians(110.f), glm::normalize(glm::vec3(0.1f,0.2f,1.f))), glm::vec3(0.3f,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(0.27f,0,0) };
	// target chosen just inside reach (Lab+Lcb = 0.57)
	EXPECT_LT(solve_error(A, B, C, glm::vec3(0.40f, 1.4f, 0.2f)), kEps);
}

TEST(TwoBoneIkTest, NearStraightArm) {
	Bone A{ glm::quat(1,0,0,0), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(3.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	EXPECT_LT(solve_error(A, B, C, glm::vec3(1.2f, 0.9f, 0.4f)), kEps);
}

// Unreachable target: the arm extends straight at the target. The invariant is that the
// end effector lies on the a->target ray (direction matches); the residual distance is
// whatever is left over beyond the arm's reach, which we don't pin down exactly because
// the solver intentionally keeps a tiny safety bend so the arm never fully straightens.
TEST(TwoBoneIkTest, UnreachableExtendsTowardTarget) {
	Bone A{ glm::quat(1,0,0,0), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	const glm::vec3 target(3.0f, 1.0f, 0.5f);
	glm::vec3 a, c;
	solve(A, B, C, target, a, c);
	const float reach = 2.0f;
	EXPECT_NEAR(glm::length(c - a), reach, 0.02f); // ~fully extended (minus safety eps)
	EXPECT_GT(glm::dot(glm::normalize(c - a), glm::normalize(target - a)), 0.9999f); // aimed at target
}

// The pole/joint target must actually steer which side the elbow bends towards (mirrors
// Unreal's Two Bone IK Joint Target), not just tie-break a perfectly straight arm.
TEST(TwoBoneIkTest, PoleVectorSteersBendDirection) {
	Bone A{ glm::quat(1,0,0,0), glm::vec3(0,0,0) };
	Bone B{ glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)), glm::vec3(1,0,0) };
	Bone C{ glm::quat(1,0,0,0), glm::vec3(1,0,0) };
	const glm::vec3 target(1.0f, 0.f, 1.4f); // out of the initial a-b-c (xy) plane

	glm::vec3 a, b_up, c;
	solve_with_pole(A, B, C, target, glm::vec3(0.f, 5.f, 0.f), a, b_up, c);
	EXPECT_LT(glm::length(c - target), kEps); // still reaches the target

	glm::vec3 b_down;
	solve_with_pole(A, B, C, target, glm::vec3(0.f, -5.f, 0.f), a, b_down, c);
	EXPECT_LT(glm::length(c - target), kEps);

	// Elbow should sit on the pole's side of the aim axis in both cases.
	EXPECT_GT(b_up.y, a.y);
	EXPECT_LT(b_down.y, a.y);
	EXPECT_GT(b_up.y - b_down.y, 0.5f); // meaningfully different, not a coincidence
}
