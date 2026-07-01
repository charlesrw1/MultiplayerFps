#include <gtest/gtest.h>
#include "Framework/MathLib.h"
#include <algorithm>

// Geometry that backs the 2D blend space (agBlendSpace2D): Delaunay triangulation over the
// sample grid, and barycentric weights used to blend the 3 corner samples of the triangle
// containing the current input point.

TEST(Barycentric2D, InsideTriangleWeightsSumToOneAndAreNonNegative) {
	const glm::vec2 a(0.f, 0.f), b(1.f, 0.f), c(0.f, 1.f);
	float u, v, w;
	const bool inside = barycentric_2d(a, b, c, glm::vec2(0.25f, 0.25f), u, v, w);
	EXPECT_TRUE(inside);
	EXPECT_GE(u, 0.f);
	EXPECT_GE(v, 0.f);
	EXPECT_GE(w, 0.f);
	EXPECT_NEAR(u + v + w, 1.f, 0.0001f);
}

TEST(Barycentric2D, VertexGetsFullWeight) {
	const glm::vec2 a(0.f, 0.f), b(2.f, 0.f), c(0.f, 2.f);
	float u, v, w;
	ASSERT_TRUE(barycentric_2d(a, b, c, a, u, v, w));
	EXPECT_NEAR(u, 1.f, 0.0001f);
	EXPECT_NEAR(v, 0.f, 0.0001f);
	EXPECT_NEAR(w, 0.f, 0.0001f);
}

TEST(Barycentric2D, OutsideTriangleReportsFalseButStillComputesWeights) {
	const glm::vec2 a(0.f, 0.f), b(1.f, 0.f), c(0.f, 1.f);
	float u, v, w;
	const bool inside = barycentric_2d(a, b, c, glm::vec2(5.f, 5.f), u, v, w);
	EXPECT_FALSE(inside);
	// still a valid affine combination (sums to 1), just with a negative component
	EXPECT_NEAR(u + v + w, 1.f, 0.0001f);
	EXPECT_TRUE(u < 0.f || v < 0.f || w < 0.f);
}

TEST(DelaunayTriangulate2D, FewerThanThreePointsProducesNoTriangles) {
	EXPECT_TRUE(delaunay_triangulate_2d({}).empty());
	EXPECT_TRUE(delaunay_triangulate_2d({glm::vec2(0, 0)}).empty());
	EXPECT_TRUE(delaunay_triangulate_2d({glm::vec2(0, 0), glm::vec2(1, 1)}).empty());
}

TEST(DelaunayTriangulate2D, SingleTriangleForThreePoints) {
	std::vector<glm::vec2> pts = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0, 1)};
	auto tris = delaunay_triangulate_2d(pts);
	ASSERT_EQ(tris.size(), 1u);
	// all three input indices are used exactly once
	std::vector<int> idx = {tris[0].a, tris[0].b, tris[0].c};
	std::sort(idx.begin(), idx.end());
	EXPECT_EQ(idx[0], 0);
	EXPECT_EQ(idx[1], 1);
	EXPECT_EQ(idx[2], 2);
}

TEST(DelaunayTriangulate2D, SquareGridCoversWholeAreaWithNoOverlap) {
	// A 2x2 grid of samples (like a walk/run x strafe blend space) triangulates into 2
	// triangles that partition the square with no gaps: every interior point should land
	// inside exactly one triangle.
	std::vector<glm::vec2> pts = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)};
	auto tris = delaunay_triangulate_2d(pts);
	ASSERT_EQ(tris.size(), 2u);

	auto count_containing = [&](glm::vec2 p) {
		int count = 0;
		for (auto& t : tris) {
			float u, v, w;
			if (barycentric_2d(pts[t.a], pts[t.b], pts[t.c], p, u, v, w))
				count++;
		}
		return count;
	};
	// Points exactly on the shared diagonal (whichever one the triangulator picks) are a
	// legitimate boundary case counted as "inside" by both triangles, so this deliberately
	// avoids the two diagonals (x==y and x+y==1) and checks points clearly on one side.
	EXPECT_EQ(count_containing(glm::vec2(0.1f, 0.2f)), 1);
	EXPECT_EQ(count_containing(glm::vec2(0.9f, 0.8f)), 1);
	EXPECT_EQ(count_containing(glm::vec2(0.8f, 0.1f)), 1);
	EXPECT_EQ(count_containing(glm::vec2(0.2f, 0.9f)), 1);
}
