layout(binding = 0)	uniform sampler2D g_FullResDepth;

layout(location = 0) in vec2 i_UV;

layout(location = 0) out float outDepth;

void main()
{
    const vec4 depths = textureGather(g_FullResDepth, i_UV);

	float depth = depths[0];
	for (int i = 1; i < 4; i++)
	{
		// taking further depth, because for GTAO:
		// reduces halo
		// decrease AO casted by thin objects
		if (depths[i] < depth)
		{
			depth = depths[i];
		}
	}

	outDepth = depth;
}
