#extension GL_EXT_nonuniform_qualifier : enable
#include "particle_system/common.h"

#define EG_NO_MATERIALS
#include "pipeline_layout.h"

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
	if (i_TextureIndex != EG_INVALID_INDEX)
	{
		vec4 texColor;
		if (HasFlag(i_Flags, Particle_BlendAnimation_Mask))
		{
			const vec4 color0 = ReadTexture_sRGB(i_TextureIndex, i_UVs.xy);
			const vec4 color1 = ReadTexture_sRGB(i_TextureIndex, i_UVs.zw);
			texColor = mix(color0, color1, i_AnimationLerp);
		}
		else
		{
			texColor = ReadTexture_sRGB(i_TextureIndex, i_UVs.xy);
		}

#ifndef EG_BLEND
		// Alpha-mask the texture so that sprites aren't rendered as solid quads.
		// Note: only the texture's alpha is tested, so the emitter's color alpha has no effect on it
		if (texColor.a < EG_OPACITY_MASK_THRESHOLD)
			discard;
#endif
		color *= texColor;
	}
#ifdef EG_BLEND
	color.rgb *= color.a;
	if (HasFlag(i_Flags, Particle_Additive_Mask))
		color.a = 0.f; // To not dim Dst Color
#endif
	o_Color = color;
}
