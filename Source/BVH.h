#pragma once
#include "MathLib.h"
#include <vector>
#include <algorithm>

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

	std::vector<BVHNode> nodes;
	std::vector<int> indicies;
};

class BVHBuilder
{
public:
	BVHBuilder(const std::vector<Bounds>& bounds, int max_per_leaf) : bounds(bounds), max_per_leaf(max_per_leaf)
	{
		indicies.resize(bounds.size());
		centroids.resize(bounds.size());
		for (int i = 0; i < bounds.size(); i++)
			indicies[i] = i;
		for (int i = 0; i < bounds.size(); i++)
			centroids[i] = bounds[i].get_center();
	}
	void build_R(int start, int end, int node_num);

	int add_new_node() {
		nodes.resize(nodes.size() + 1);
		return nodes.size() - 1;
	}
	Bounds calc_bounds(int start, int end) {
		Bounds b;
		for (int i = start; i < end; i++)
			b = bounds_union(b, bounds[indicies[i]]);
		return b;
	}

	float EvalSAH(int start, int end, int axis, float pos) const {
		Bounds left, right;
		int left_count = 0, right_count = 0;
		for (int i = start; i < end; i++)
		{
			if (centroids[indicies[i]][axis] < pos) {
				left_count++;
				left = bounds_union(left, bounds[indicies[i]]);
			}
			else {
				right_count++;
				right = bounds_union(right, bounds[indicies[i]]);
			}
		}
		float cost = left_count * left.surface_area() + right_count * right.surface_area();
		return cost > 0 ? cost : 1e30f;
	}

	std::vector<vec3> centroids;
	std::vector<BVHNode> nodes;
	std::vector<int> indicies;
	const std::vector<Bounds>& bounds;

	int max_per_leaf;
};

struct Bin
{
	Bounds aabb;
	int tri_count = 0;
};
static const int NUM_BINS = 20;

#ifndef  _DEBUG 
#define SAH
#endif // ! _DEBUG 

void BVHBuilder::build_R(int start, int end, int node_number)
{
	assert(start >= 0 && end <= bounds.size());
	int num_elements = end - start;
	BVHNode* node = &nodes[node_number];
	node->aabb = calc_bounds(start, end);

	if (num_elements <= max_per_leaf) {
		node->left_node = start;
		node->count = num_elements;
		return;
	}
	int split = start;

#ifndef SAH

	vec3 bounds_center = node->aabb.get_center();
	int longest_axis = node->aabb.longest_axis();
	float mid = bounds_center[longest_axis];

	auto split_iter = std::partition(indicies.begin() + start, indicies.begin() + end,
		[longest_axis, mid, this](int index) {
			return this->centroids[index][longest_axis] < mid;
		}
	);
	split = split_iter - indicies.begin();
	if (split == start || split == end)
		split = (start + end) / 2;
#else
	float parent_cost = node->aabb.surface_area() * num_elements;

	Bounds centroid_bounds;
	for (int i = start; i < end; i++) {
		centroid_bounds = bounds_union(centroid_bounds, centroids[indicies[i]]);
	}


	float mid;
	int axis = -1;
	float best_cost = 1e30f;
	for (int a = 0; a < 3; a++) {
		float bounds_min = centroid_bounds.bmin[a];// node->aabb.bmin[a];
		float bounds_max = centroid_bounds.bmax[a];// node->aabb.bmax[a];
		if (bounds_min == bounds_max)continue;
		Bin bins[NUM_BINS];
#ifdef NO_BIN
		float scale = (bounds_max - bounds_min) / 100.f;
		for (int i = 0; i < 100; i++) {
			//float centroid = centroids[indicies[start + i]][a];
			float pos = bounds_min + i * scale;
			float cost = EvalSAH(start, end, a, pos);
			if (cost < best_cost) {
				mid = pos;
				axis = a;
				best_cost = cost;
			}
		}
#endif // NO_BIN
		float scale = NUM_BINS / (bounds_max - bounds_min);
		for (int i = start; i < end; i++) {
			vec3 centroid = centroids[indicies[i]];
			int bin_idx = glm::min(NUM_BINS - 1, (int)((centroid[a] - bounds_min) * scale));
			bins[bin_idx].tri_count++;
			bins[bin_idx].aabb = bounds_union(bins[bin_idx].aabb, bounds[indicies[i]]);
		}

		float left_area[NUM_BINS - 1], right_area[NUM_BINS - 1];
		int left_count[NUM_BINS - 1], right_count[NUM_BINS - 1];
		Bounds left_aabb, right_aabb;
		int left_sum = 0, right_sum = 0;
		for (int i = 0; i < NUM_BINS - 1; i++)
		{
			left_sum += bins[i].tri_count;
			left_count[i] = left_sum;
			left_aabb = bounds_union(left_aabb, bins[i].aabb);
			left_area[i] = left_aabb.surface_area();

			right_sum += bins[NUM_BINS - 1 - i].tri_count;
			right_count[NUM_BINS - 2 - i] = right_sum;
			right_aabb = bounds_union(right_aabb, bins[NUM_BINS - 1 - i].aabb);
			right_area[NUM_BINS - 2 - i] = right_aabb.surface_area();
		}

		scale = (bounds_max - bounds_min) / NUM_BINS;
		for (int i = 0; i < NUM_BINS - 1; i++) {
			float plane_cost = left_count[i] * left_area[i] + right_count[i] * right_area[i];
			if (plane_cost < best_cost) {
				mid = bounds_min + scale * (i + 1);
				axis = a;
				best_cost = plane_cost;
			}
		}

	}

	if (axis == -1 || best_cost >= parent_cost) {
		node->left_node = start;
		node->count = num_elements;
		return;
	}

	auto split_iter = std::partition(indicies.begin() + start, indicies.begin() + end,
		[axis, mid, this](int index) {
			return this->centroids[index][axis] < mid;
		}
	);
	split = split_iter - indicies.begin();

	if (split == start || split == end)
		split = (start + end) / 2;



#endif // !SAH

	node->count = BVH_BRANCH;
	// These may invalidate the pointer
	int left_child = add_new_node();
	int right_child = add_new_node();
	// So just set it again lol
	nodes[node_number].left_node = left_child;
	build_R(start, split, left_child);
	build_R(split, end, right_child);
}

// Static builder function
BVH BVH::build(const std::vector<Bounds>& bounds, int max_per_node, PartitionStrategy strat)
{
	BVHBuilder builder(bounds, max_per_node);

	int start_node = builder.add_new_node();	// =0
	builder.build_R(0, bounds.size(), start_node);

	if (builder.nodes.size() == 0) {
		printf("Error on BVH construction\n");
		return BVH();
	}
	BVH bvh;
	bvh.nodes = std::move(builder.nodes);
	bvh.indicies = std::move(builder.indicies);

	return bvh;
}