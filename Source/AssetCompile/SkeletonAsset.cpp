#include "SkeletonAsset.h"

CLASS_IMPL(SkeletonMask);
CLASS_IMPL(SkeletonMirror);
CLASS_IMPL(SkeletonRagdoll);


// SKELETON BIN FORMAT
// int num_bones
// mat4 bindpose[num_bones]
// mat4 invbindpose[num_bones]
// string names[num_bones]
// mat4 localtransform[num_bones]

// Unified asset format: (models,animations,skeleton,masks,mirror)
// int magic 'U' 'N' 'I' 'F'
// int version
// int name_len
// char name[name_len+1] (w/ null terminator)
// int prop_size
// char properties[]
// int binary_section_len
// char binary_data[binary_secion_len]

// MASK BIN FORMAT
// empty


// if high bit set, then channel has only one frame
// CHANNEL_OFS
// int pos_ofs
// int rot_ofs
// int scale_ofs

// float* packed_data[ pos_ofs + 3*keyframe ] 

// ANIMATION 
// string name
// float duration
// float linear_velocity
// int num_keyframes
// bool is_delta
// 
// CHANNEL_OFS channels[num_bones]
// 
// int packed_size
// float packed_data[packed_size]
// 
// int num_events
// EVENT events[num_events]
//		"{ type <event_type> <fields> }"
// int num_curves

// BAKEDCURVE
// int curve_data_count
// 
// float[curve_data_count]
