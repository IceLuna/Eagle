#extension GL_EXT_nonuniform_qualifier : enable
#include "particle_system/common.h"

layout(set = 1, binding = 0) uniform sampler2D g_Textures[];

// Inputs
layout(location = 0) in vec4 i_Color;
layout(location = 1) in vec4 i_UVs;
layout(location = 2) flat in uint i_TextureIndex;
layout(location = 3) flat in uint i_Flags;
layout(location = 4) in float i_AnimationLerp;

// Outputs
layout(location = 0) out vec4 o_Color;

void main()
{
	vec4 color = i_Color;
	if (i_TextureIndex != 0)
	{
		if (HasFlag(i_Flags, Particle_BlendAnimation_Mask))
		{
			const vec4 color0 = texture(g_Textures[nonuniformEXT(i_TextureIndex)], i_UVs.xy);
			const vec4 color1 = texture(g_Textures[nonuniformEXT(i_TextureIndex)], i_UVs.zw);
			color *= mix(color0, color1, i_AnimationLerp);
		}
		else
		{
			color *= texture(g_Textures[nonuniformEXT(i_TextureIndex)], i_UVs.xy);
		}
	}
#ifdef EG_BLEND
	color.rgb *= color.a;
	if (HasFlag(i_Flags, Particle_Additive_Mask))
		color.a = 0.f; // To not dim Dst Color
#endif
	o_Color = color;
}
