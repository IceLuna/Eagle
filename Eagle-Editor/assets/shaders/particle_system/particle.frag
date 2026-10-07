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

#ifdef EG_BLEND
layout(location = 5) in float i_ViewDistance;
layout(location = 6) flat in vec3 i_FadeParams; // x - depth fade distance (0 - off), y/z - camera fade from/to distance (y >= z - off)
layout(location = 7) flat in vec2 i_DepthParams;

layout(binding = 3) uniform sampler2D g_Depth;

// Perspective projections give `clip.z = A * z + B` and `clip.w = -z`,
// so `depth = -A - B / z`, and the distance `-z = B / (depth + A)`
float DepthToViewDistance(float depth)
{
	return i_DepthParams.y / (depth + i_DepthParams.x);
}
#endif

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
	// Depth fade. Fade out as the particle gets close to the geometry behind it
	if (i_FadeParams.x > 0.f)
	{
		const float sceneDistance = DepthToViewDistance(texelFetch(g_Depth, ivec2(gl_FragCoord.xy), 0).r);
		color.a *= clamp((sceneDistance - i_ViewDistance) / i_FadeParams.x, 0.f, 1.f);
	}

	// Camera fade. Fully faded at `y` from the camera, fully visible from `z`
	if (i_FadeParams.y < i_FadeParams.z)
		color.a *= clamp((i_ViewDistance - i_FadeParams.y) / (i_FadeParams.z - i_FadeParams.y), 0.f, 1.f);

	color.rgb *= color.a;
	if (HasFlag(i_Flags, Particle_Additive_Mask))
		color.a = 0.f; // To not dim Dst Color
#endif
	o_Color = color;
}
