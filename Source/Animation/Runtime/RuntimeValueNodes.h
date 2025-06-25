#pragma once
#include "Framework/PropertyPtr.h"
#include "RuntimeNodesNew.h"
#include <variant>
using std::variant;
using glm::vec3;
using std::string;

class atInitContext;
class atValueNode : public ClassBase {
public:
	CLASS_BODY(atValueNode);
	enum Type  :int8_t{
		Float,Bool,Int,Vector3
	};
	REF int8_t typeInt = Bool;

	virtual void init(atInitContext& ctx) {}
	virtual float get_float(atUpdateStack& ctx) const { return 0.f; }
	virtual bool get_bool(atUpdateStack& ctx) const { return false; }
	virtual int get_int(atUpdateStack& ctx) const { return 0; }
	virtual vec3 get_vector3(atUpdateStack& ctx) const { return vec3(0.f); }
	Type get_out_type() const { return (Type)typeInt; }
};
class atBreakVectorNode : public atValueNode {
public:
	CLASS_BODY(atBreakVectorNode);
	float get_float(atUpdateStack& ctx) const final {
		auto vec = vectorPtr->get_vector3(ctx);
		assert(index >= 0 && index < 2);
		return vec[index];
	}
	void init(atInitContext& ctx) final {
		assert(index >= 0 && index < 2);
		vectorPtr = ctx.find_value(vecid);
	}
	atValueNode* vectorPtr = nullptr;
	REF int vecid = 0;
	REF int index = 0;
};
class atMakeVectorNode : public atValueNode {
public:
	CLASS_BODY(atMakeVectorNode);
	REF int node0 = 0;
	REF int node1 = 0;
	REF int node2 = 0;
	atValueNode* ptr0 = nullptr;
	atValueNode* ptr1 = nullptr;
	atValueNode* ptr2 = nullptr;
	void init(atInitContext& ctx) final {
		ptr0 = ctx.find_value(node0);
		ptr1 = ctx.find_value(node1);
		ptr2 = ctx.find_value(node2);
	}
	vec3 get_vector3(atUpdateStack& ctx) const final {
		assert(ptr0 && ptr1 && ptr2);
		return vec3(ptr0->get_float(ctx), ptr1->get_float(ctx), ptr2->get_float(ctx));
	}
};

class atVariableNode : public atValueNode {
public:
	CLASS_BODY(atVariableNode);
	void init(atInitContext& ctx) final;
	bool get_bool(atUpdateStack& ctx)const final;
	float get_float(atUpdateStack& ctx)const final;
	int get_int(atUpdateStack& ctx)const final;
	vec3 get_vector3(atUpdateStack& ctx)const final;

	REF string varName;
	const PropertyInfo* p = nullptr;
};
class atConstantNode : public atValueNode {
public:
	CLASS_BODY(atConstantNode);

	variant<bool, int, float, vec3> values;
};

enum class MathNodeType
{
	Add, Sub, Mult, Div, Lt, Gt, Leq, Geq, Eq, Neq
};


class atMathNode : public atValueNode {
public:
	CLASS_BODY(atMathNode);
	void init(atInitContext& ctx) final {
		lptr = ctx.find_value(leftid);
		rptr = ctx.find_value(rightid);
	}
	REF int8_t opTypeInt = 0;	// MathNodeType
	REF int8_t inputTypeInt = 0;	// atValueNode::Type
	MathNodeType get_optype() const { return MathNodeType(opTypeInt); }
	bool get_bool(atUpdateStack& ctx) const final;
	float get_float(atUpdateStack& ctx) const final;
	int get_int(atUpdateStack& ctx) const final;
	vec3 get_vector3(atUpdateStack& ctx) const final;


	REF int leftid = 0;
	REF int rightid = 0;
	atValueNode* lptr = nullptr;
	atValueNode* rptr = nullptr;
};
class atNotNode : public atValueNode {
public:
	CLASS_BODY(atNotNode);
	void init(atInitContext& ctx) final {
		ptr = ctx.find_value(input);
	}
	bool get_bool(atUpdateStack& ctx) const final {
		return !ptr->get_bool(ctx);
	}
	REF int input = 0;
	atValueNode* ptr = nullptr;
};

class atLogicalOpNode : public atValueNode {
public:
	CLASS_BODY(atLogicalOpNode);
	bool get_bool(atUpdateStack& ctx) const final;
	void init(atInitContext& ctx) final {
		ptrs.clear();
		for (int id : nodes) {
			ptrs.push_back(ctx.find_value(id));
		}
	}

	REF vector<int> nodes;
	REF bool is_and = false;
	vector<atValueNode*> ptrs;
};

