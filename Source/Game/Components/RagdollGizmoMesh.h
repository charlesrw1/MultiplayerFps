#pragma once
#include "glm/glm.hpp"

class ModelBuilder;

// Solid-mesh builders for RagdollJointComponent's degrees-of-freedom gizmo. These append real
// triangles (via ModelBuilder) so the gizmo can be shaded with a front/back-facing material
// (see eng/ragdollJointGizmo.mm) instead of drawn as debug lines.

// Orthonormal (tangent, bitangent) basis perpendicular to dir; up_ref picks a consistent
// bitangent direction (falls back to a fixed axis if parallel to dir).
void ragdoll_make_basis(glm::vec3 dir, glm::vec3 up_ref, glm::vec3& tangent, glm::vec3& bitangent);

// Maps independent swing1/swing2 angle limits (radians) to an elliptical cone rim radius at
// `length` from the apex, clamping both the visualized angle and the resulting radius so
// near-90-degree limits (common on hinge-like joints) don't blow up the gizmo size.
void ragdoll_cone_radii(float angle_y, float angle_z, float length, float& out_ry, float& out_rz);

// Elliptical swing-limit cone solid, apex at `apex`, opening toward `dir`. angle_y/angle_z are
// the swing1/swing2 limits (radians), matching AdvancedJointComponent::set_cone_vars semantics
// (ang_y_limit, ang_z_limit).
void ragdoll_append_cone_solid(ModelBuilder& mb, glm::vec3 apex, glm::vec3 dir, float angle_y, float angle_z,
								float length);

// Flat pie-slice/fan solid spanning [min_rad, max_rad] around `axis`, starting from `zero_ref`
// (must be perpendicular to axis), given a small `thickness` extrusion so both faces are real
// front/back-facing triangles (for the inside/outside material read) rather than a coincident
// double-sided quad. Used for the twist semicircle and the single-axis swing (hinge) case.
void ragdoll_append_wedge_solid(ModelBuilder& mb, glm::vec3 center, glm::vec3 axis, glm::vec3 zero_ref,
								 float min_rad, float max_rad, float radius, float thickness);
