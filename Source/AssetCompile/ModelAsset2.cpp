#include "ModelAsset2.h"


CLASS_IMPL(AnimImportSettings);
CLASS_IMPL(ModelImportSettings);

// MODEL FORMAT:
// HEADER
// int magic 'C' 'M' 'D' 'L'
static const int MODEL_VERSION = 7;
// int version XXX
// 
// int reflected_properties_len 
// char reflected_properties_str[reflected_properties_len] (this gets parsed as a ModelAsset object)
// 
// mat4 root_transform
// Bounds aabb
// 
// int num_lods
// LOD lods[num_lods]
// 
// int num_meshes
// Submesh meshes[num_meshes]
// 
// 
// VBIB vbib
// 
// **PHYSICS DATA:
// bool has_physics (if false, skip this section)
// bool can_be_dynamic
// int num_bodies
// PSubBodyDef bodies[num_bodies]
// int num_shapes
// physics_shape_def shapes[num_shapes] (pointers are serialized as an offset to { int size, data[] })
// int num_constraints
// PhysicsBodyConstraintDef constrains[num_contraints]
// 


// MESH
// int base_vertex
// int element_offset
// int element_count
// int material_index

// LOD
// float screen_size
// int num_meshes
// MESH meshes[num_meshes] 

// VERTEX
// vec3 pos
// vec2 uv
// int16 normals
// int16 tangents
// int8[4] color
// int8[4] color2

// VBIB
// int num_verticies
// int num_indicies
// int16 indicies[num_indicies]
// VERTEX verticies[num_verticies]

// LOCATOR
// vec3 position
// quat rotation
// int bone_index
// string name
