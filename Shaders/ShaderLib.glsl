// Useful shader functions #include this inside _FS_BEGIN _FS_END in your master shader

// returns linear depth from NDC
float linearize_depth(float depth)
{
	return g.near / depth;
}

// returns linear depth of fragment in depth texture
float get_depth_texture_depth(in sampler2D depth_texture)
{
	return linearize_depth(texture(depth_texture, FS_IN_NDC.xy/FS_IN_NDC.w * vec2(0.5) + vec2(0.5)).r);
}

// returns linear depth of current fragment
float get_fragment_depth()
{
	return linearize_depth(FS_IN_NDC.z/FS_IN_NDC.w);
}
