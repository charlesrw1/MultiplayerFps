#pragma once
#include "AnimTreeAsset.h"
#include <string>
#include <unordered_map>

class agBaseNode;
class agBuilder;

// Turns an AnimTreeAsset's data into a real agBaseNode graph inside `builder`. This is the
// "manual plumbing" util: it only allocates and wires the nodes described by the asset. The
// caller still owns constructing agBuilder/AnimatorObject themselves, and can look up any
// named AnimTreeNode's runtime node in `named_nodes` to manually parent custom C++/statemachine
// logic onto it afterward.
namespace AnimTreeBuild
{
	struct BuildResult
	{
		agBaseNode* root = nullptr;
		// Only nodes with a non-empty AnimTreeNode::name are present here.
		std::unordered_map<std::string, agBaseNode*> named_nodes;
	};

	BuildResult build(const AnimTreeAsset& asset, agBuilder& builder);
}
