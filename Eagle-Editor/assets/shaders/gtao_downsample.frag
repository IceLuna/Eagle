layout(binding = 0)	uniform sampler2D g_FullResDepth;
layout(binding = 1)	uniform sampler2D g_FullResMotion;

layout(location = 0) in vec2 i_UV;

layout(location = 0) out float outDepth;
layout(location = 1) out vec2 outMotion;

void main()
{
    const vec4 depths = textureGather(g_FullResDepth, i_UV);
	const vec4 velXs  = textureGather(g_FullResMotion, i_UV, 0);
	const vec4 velYs  = textureGather(g_FullResMotion, i_UV, 1);

	float depth = depths[0];
	vec2 velocity = vec2(velXs[0], velYs[0]);
	for (int i = 1; i < 4; i++) {
		// taking further depth, because for GTAO:
		// reduces halo
		// decrease AO casted by thin objects
		if (depths[i] < depth) {
			depth = depths[i];
			velocity = vec2(velXs[i], velYs[i]);
		}
	}

	outDepth = depth;
	outMotion = velocity;
}
