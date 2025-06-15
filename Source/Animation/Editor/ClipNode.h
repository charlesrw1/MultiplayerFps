#pragma once
#include "Base_node.h"
#include <variant>
using std::variant;


struct ClipNode_SData
{
	STRUCT_BODY(ClipNode_SData);
	REF AssetPtr<AnimationSeqAsset> Clip;
	REF bool loop = true;
	REF StringName SyncGroup;
	REF sync_opt SyncOption = sync_opt::Default;
	bool has_sync_group() const {
		return !SyncGroup.is_null();
	}
	REF float speed = 1.0;
	REF int start_frame = 0;
};

class Clip_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Clip_EdNode);
	REF ClipNode_SData Data;
};

class Variable_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Variable_EdNode);
	REF StringName Variable;
};

class Blend2_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Blend2_EdNode);
};
class BlendInt_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(BlendInt_EdNode);
	BlendInt_EdNode() = default;
};

class ConstValue_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(ConstValue_EdNode);
	variant<bool, int64_t, float, glm::vec3, glm::quat> values;
	void serialize(Serializer& s) final {}
};


class Func_EdNode : public Base_EdNode
{
public:
};

class BreakMake_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(BreakMake_EdNode);

	BreakMake_EdNode() = default;
	BreakMake_EdNode(bool make, bool is_vec3) {}
private:

};