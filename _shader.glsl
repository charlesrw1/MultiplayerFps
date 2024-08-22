// ***********************************
// **** GENERATED MATERIAL SHADER ****
// ***********************************
#define ALPHATEST

// ******** START "include "SharedGpuTypes.txt"" ********

#ifndef SHAREDGPU_
#define SHAREDGPU_

#ifdef __cplusplus
using namespace glm;
typedef uint32_t uint;
namespace gpu {
#endif

struct Object_Instance
{
	mat4 model;
	mat4 invmodel;
	vec4 colorval;
	uint anim_mat_offset;
	uint opposite_dither;
	uint colorval2;
	float obj3;
};

// flags for 'bitmask_flags'
const uint MATFLAG_BILLBOARD_ROTATE_AXIS  = 1;

struct Material_Data
{
	vec4 diffuse_tint;
	float rough_mult;
	float metal_mult;
	float padding;
	uint bitmask_flags;
};

const uint DEBUG_NONE = 0;
const uint DEBUG_NORMAL = 1;
const uint DEBUG_MATID = 2;
const uint DEBUG_AO = 3;
const uint DEBUG_WIREFRAME = 4;
const uint DEBUG_ALBEDO = 5;
const uint DEBUG_DIFFUSE = 6;
const uint DEBUG_SPECULAR = 7;
const uint DEBUG_OBJID = 8;
const uint DEBUG_LIGHTING_ONLY = 9;



struct Ubo_View_Constants_Struct
{
	mat4 view;
	mat4 viewproj;
	mat4 invview;
	mat4 invproj;
	mat4 inv_viewproj;
	vec4 viewpos_time;
	vec4 viewfront;
	vec4 viewport_size;

	float near;
	float far;
	float shadowmap_epsilon;
	float inv_scale_by_proj_distance;
	
	vec4 fogcolor;
	vec4 fogparams;
	vec4 directional_light_dir_and_used;
	vec4 directional_light_color;
	
	float numcubemaps;
	float numlights;
	float forcecubemap;
	uint debug_options;
	
	vec4 custom_clip_plane;
};

struct DispatchIndirectCommand 
{
	uint  num_groups_x;
	uint  num_groups_y;
	uint  num_groups_z;
};

struct DrawElementsIndirectCommand 
{
	uint count;
	uint primCount;
	uint firstIndex;
	int  baseVertex;
	uint baseInstance;
};

struct Chunk 
{
    vec4 cone_apex;
    vec4 cone_axis_cutoff;
    vec4 bounding_sphere;
    uint index_offset;
    uint index_count;
	uint padding1;
	uint padding2;
};

struct Cull_Paramaters
{
	mat4 view;
	mat4 proj;
	vec4 viewpos;
	vec4 frustum;
	
	float near;
	float far;
	int cull_instances;
	int cull_cone;
	
	int cull_frustum;
	int num_instances;
	int padding1;
	int padding2;
};


#define AO_RANDOMTEX_SIZE 4
// from nvidias gl_ssao
struct HBAOData {
  float   RadiusToScreen;	// radius
  float   R2;     			// 1/radius
  float   NegInvR2;     	// radius * radius
  float   NDotVBias;
 
  vec2    InvFullResolution;
  vec2    InvQuarterResolution;
  
  float   AOMultiplier;
  float   PowExponent;
  vec2    _pad0;
  
  vec4    projInfo;
  vec2    projScale;
  int     projOrtho;
  int     _pad1;
  
  vec4    float2Offsets[AO_RANDOMTEX_SIZE*AO_RANDOMTEX_SIZE];
  vec4    jitters[AO_RANDOMTEX_SIZE*AO_RANDOMTEX_SIZE];
};

struct Post_Process_Settings
{
	float exposure;
	float contrast;
};

#ifdef __cplusplus
};
#endif

#endif

// ******** END "include "SharedGpuTypes.txt"" ********

layout (binding = 0, std140) uniform Ubo_View_Constant_Buffer {
	Ubo_View_Constants_Struct g;
};

layout (binding = 2, std140) readonly buffer Object_Data_Buffer {
	Object_Instance g_objects[];
};
layout (binding = 3, std140) readonly buffer Object_Skin_Matricies {
	mat4 g_skin_matricies[];
};

layout (binding = 5, std430) readonly buffer Indirect_Instance_Buf {
	uint indirect_instance[];
};
layout(binding = 6, std430) readonly buffer Indirect_Mat_Buf {
	uint indirect_materials[];
};
layout (binding = 4, std430) readonly buffer GlobalMaterialBuffer {
	uint _material_param_buffer[];
};

#ifdef _VERTEX_SHADER


layout (location = 0) in vec3 VS_IN_Postion;
layout (location = 1) in vec2 VS_IN_TexCoord;
layout (location = 2) in vec3 VS_IN_Normal;
layout (location = 3) in vec3 VS_IN_Tangent;

layout (location = 4) in ivec4 VS_IN_BoneIndicies;
layout (location = 5) in vec4 VS_IN_BoneWeights;


flat out uint FS_IN_Objid;
flat out uint FS_IN_Matid;
out vec3 FS_IN_FragPos;
out vec3 FS_IN_Normal;
out vec2 FS_IN_Texcoord;
out vec3 FS_IN_BoneColor;
out vec4 FS_IN_NDC;

out mat3 FS_IN_TBN;

vec3 randColor(int number){
    return fract(sin(vec3(number+1)*vec3(12.8787, 1.97, 20.73739)));
}


uniform int indirect_material_offset = 0;

// standard vertex shader outputs
vec3 WORLD_POSITION_OFFSET = vec3(0.0);
mat4 ObjModelMatrix;
mat4 ObjInvModelMatrix;

// ********** START USER CODE **********
// Texture defs
layout(binding = 0 ) uniform sampler2D Sprite;

void VSmain()
// ********** END USER CODE **********

void main()
{
	FS_IN_Matid = indirect_materials[indirect_material_offset + gl_DrawID];
	uint obj_index = indirect_instance[gl_BaseInstance + gl_InstanceID]; 
	FS_IN_Objid = obj_index;

	ObjModelMatrix = g_objects[obj_index].model;
	ObjInvModelMatrix = g_objects[obj_index].invmodel;

	vec3 local_pos = vec3(0.0);
	vec3 local_normal = vec3(0.0);
	vec3 local_tangent = vec3(0.0);
#ifdef ANIMATED

	uint obj_skin_offset = g_objects[obj_index].anim_mat_offset;

	for(int i=0;i<4;i++) {
		if(VS_IN_BoneIndicies[i]==-1)
			continue;
		vec3 posadded = vec3(g_skin_matricies[obj_skin_offset + VS_IN_BoneIndicies[i]]*vec4(VS_IN_Postion,1.0));
		local_pos += posadded * VS_IN_BoneWeights[i];
		
		vec3 normaladded = mat3(g_skin_matricies[obj_skin_offset + VS_IN_BoneIndicies[i]])*VS_IN_Normal;
		local_normal += normaladded * VS_IN_BoneWeights[i];
		
		vec3 tangentadded = mat3(g_skin_matricies[obj_skin_offset + VS_IN_BoneIndicies[i]])*VS_IN_Tangent;
		local_tangent += tangentadded;
	}
	
#else	// (NOT ANIMATED)
	local_pos = VS_IN_Postion;
	local_normal = VS_IN_Normal;
	local_tangent = VS_IN_Tangent;

#endif // ANIMATED

//	vec3 local_pos = vec3(BoneTransform[VS_IN_Bone]*vec4(VS_IN_Postion,1.0));
#ifdef ANIMATED
	FS_IN_BoneColor = randColor(VS_IN_BoneIndicies.x)*VS_IN_BoneWeights.x+
		randColor(VS_IN_BoneIndicies.y)*VS_IN_BoneWeights.y+
		randColor(VS_IN_BoneIndicies.z)*VS_IN_BoneWeights.z+
		randColor(VS_IN_BoneIndicies.w)*VS_IN_BoneWeights.w;
#endif // ANIMATED

	
	FS_IN_FragPos = vec3(ObjModelMatrix * vec4(local_pos,1.0));
	
	VSmain();
	
	FS_IN_FragPos = FS_IN_FragPos + WORLD_POSITION_OFFSET;
	
    FS_IN_Normal = mat3(transpose(ObjInvModelMatrix))*normalize(local_normal);
	
	vec3 world_tangent = mat3(transpose(ObjInvModelMatrix))*normalize(local_tangent);
	vec3 world_bitangent = cross(FS_IN_Normal, world_tangent);
	FS_IN_TBN = mat3(
	normalize(local_tangent  ), 
	normalize(world_bitangent), 
	normalize(FS_IN_Normal   ));
    
    FS_IN_NDC = g.viewproj * vec4(FS_IN_FragPos, 1.0);

	gl_Position = FS_IN_NDC;
	FS_IN_Texcoord = VS_IN_TexCoord;
}

#endif // _VERTEX_SHADER

#ifdef _FRAGMENT_SHADER

layout (location = 0) out vec3 GBUFFER_OUT_Normal;
layout (location = 1) out vec4 GBUFFER_OUT_Albedo_AO;
layout (location = 2) out vec4 GBUFFER_OUT_M_R_Custom_Matid;
layout (location = 3) out vec4 GBUFFER_OUT_Emissive;

#ifdef EDITOR_ID
layout(location = 4) out vec4 GBUFFER_OUT_EditorID;
#endif

flat in uint FS_IN_Objid;
flat in uint FS_IN_Matid;
in vec3 FS_IN_FragPos;
in vec3 FS_IN_Normal;
in vec2 FS_IN_Texcoord;
in vec3 FS_IN_BoneColor;
in vec4 FS_IN_NDC;


in mat3 FS_IN_TBN;

vec3 randColor(uint number){
    return fract(sin(vec3(number+1)*vec3(12.8787, 1.97, 20.73739)));
}

// standard outputs
vec3 BASE_COLOR = vec3(0.0);	// albedo
float ROUGHNESS = 0.5;
float METALLIC = 0.0;
vec3 EMISSIVE = vec3(0.0);		// emissive color, added after lighting (also main input for unlit)
vec3 NORMALMAP = vec3(0.5,0.5,1.0);		// tangent space normal map
float OPACITY = 1.0;					// opacity, for alpha test
float AOMAP = 1.0;						// small detail ao map

// ********** START USER CODE **********
// Texture defs
layout(binding = 0 ) uniform sampler2D Sprite;

void FSmain()
// ********** END USER CODE **********

void main()
{
	FSmain();
#ifdef ALPHATEST
	if(OPACITY<0.5)
		discard;
#endif // ALPHATEST

#ifndef DEPTH_ONLY

	GBUFFER_OUT_Albedo_AO = vec4(BASE_COLOR*AOMAP, AOMAP);
	GBUFFER_OUT_M_R_Custom_Matid = vec4(METALLIC,ROUGHNESS,0,0);
	GBUFFER_OUT_Emissive = vec4(EMISSIVE,1.0);
	
	vec3 Normalized_IN_N = normalize(FS_IN_Normal);
	
	// World space normal

//#ifdef NORMALMAPPED
	vec3 Final_Normal = NORMALMAP * 2 - vec3(1.0);	// 0,1 -> -1,1
	Final_Normal = normalize(FS_IN_TBN * Final_Normal); 
	GBUFFER_OUT_Normal = Final_Normal;
//#else
//	GBUFFER_OUT_Normal = Normalized_IN_N;
//#endif	// NORMALMAPED

#ifdef DEBUG_SHADER
	if(g.debug_options == DEBUG_OBJID)
		GBUFFER_OUT_Emissive.rgb = randColor(FS_IN_Objid);
	else if(g.debug_options == DEBUG_MATID)
		GBUFFER_OUT_Emissive.rgb = randColor(FS_IN_Matid+279343);
	else if(g.debug_options == DEBUG_NORMAL)
		GBUFFER_OUT_Emissive.rgb = (GBUFFER_OUT_Normal.rgb*0.5+vec3(0.5));
	else if(g.debug_options == DEBUG_ALBEDO)
		GBUFFER_OUT_Emissive.rgb = BASE_COLOR;
	else if(g.debug_options == DEBUG_WIREFRAME)
		GBUFFER_OUT_Emissive = vec4(0.0,0.5,0.5,1.0);
#endif // DEBUG_SHADER

#ifdef EDITOR_ID
	uint color_to_output = FS_IN_Objid + 1;	// to use nullptr,...
	uint R = color_to_output & 255;
	uint G = (color_to_output >> 8) & 255;
	uint B = (color_to_output >> 16) & 255;
	uint A = (color_to_output >> 24) & 255;
	
	GBUFFER_OUT_EditorID = vec4(R/255.0,G/255.0,B/255.0,A/255.0);
#endif

#endif	// DEPTH_ONLY

}

#endif // _FRAGMENT_SHADER