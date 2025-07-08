#pragma once
#include "Framework/MathLib.h"
#include <vector>

// from my previous raytracing project

enum PartitionStrategy
{
	BVH_MIDDLE,	// faster to build, worse performance
	BVH_SAH		// slow to build, better performance
};

#define BVH_BRANCH -1
struct BVHNode
{
	Bounds aabb;
	int left_node;	// points to start of indicies if count != BVH_BRANCH
	int count = BVH_BRANCH;	// if not a branch, count of indicies
};

class BVHBuilder;

class BVH
{
public:
	static BVH build(const std::vector<Bounds>& bounds, int max_per_node, PartitionStrategy strat);


	void find(glm::vec3 point, std::vector<int>& outIdx);

	std::vector<BVHNode> nodes;
	std::vector<int> indicies;
};

