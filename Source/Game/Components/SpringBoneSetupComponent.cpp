#include "SpringBoneSetupComponent.h"

SpringBoneParams SpringBoneSetupComponent::get_params() const {
	SpringBoneParams p;
	p.yaw_stiffness = yaw_stiffness;
	p.yaw_damping = yaw_damping;
	p.pitch_stiffness = pitch_stiffness;
	p.pitch_damping = pitch_damping;
	p.along_stiffness = along_stiffness;
	p.along_damping = along_damping;
	p.allow_length_flex = allow_length_flex;
	p.gravity = gravity;
	p.gravityDir = gravityDir;
	return p;
}
