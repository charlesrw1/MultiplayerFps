#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"

class atUpdateStack;
class atInitContext;
class atCreateInstContext;
class AnimTreePoseNode : public ClassBase {
public:
	CLASS_BODY(AnimTreePoseNode);
	struct Inst {
		virtual ~Inst() {}
		virtual void get_pose(atUpdateStack& context) {}
		virtual void reset() {}
	};
	virtual Inst* create_inst(atCreateInstContext& ctx) const = 0;
};
using PoseNodeInst = AnimTreePoseNode::Inst;
