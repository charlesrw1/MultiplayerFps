#include "ChannelsAndPresets.h"

CLASS_IMPL(PhysicsFilterPresetBase);
CLASS_IMPL(PP_DefaultBlockAll);
CLASS_IMPL(PP_PhysicsEntity);
CLASS_IMPL(PP_Trigger);
CLASS_IMPL(PP_Character);
CLASS_IMPL(PP_NoCollision);

PP_NoCollision::PP_NoCollision() {
	set_self(PhysicsChannel::StaticObject);
	set_default(MaskType::Ignore);
}

PP_DefaultBlockAll::PP_DefaultBlockAll() {
	set_self(PhysicsChannel::StaticObject);
	set_default(MaskType::Block);
}
PP_PhysicsEntity::PP_PhysicsEntity() {
	set_self(PhysicsChannel::PhysicsObject);
	set_default(MaskType::Block);
}
PP_Trigger::PP_Trigger() {
	set_self(PhysicsChannel::StaticObject);
	set_default(MaskType::Overlap);
	set_state(PhysicsChannel::Visiblity, MaskType::Ignore);	// dont care about overlaps with vis raycasts
}
PP_Character::PP_Character() {
	set_self(PhysicsChannel::Character);
	set_default(MaskType::Block);
	set_state(PhysicsChannel::Visiblity, MaskType::Ignore);	// dont block vis channel, use hitboxes for line traces
}