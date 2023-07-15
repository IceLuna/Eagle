#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_AtlasIndex;

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[EG_MAX_TEXTURES];

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange()
{
	const float pxRange = 2.0f;
    const vec2 unitRange = vec2(pxRange) / vec2(textureSize(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(i_TexCoords);
    return max(0.5f * dot(unitRange, screenTexSize), 1.0);
}

void main()
{
	const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords).rgb;
    const float sd = median(msd.r, msd.g, msd.b);
    const float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    const float opacity = clamp(screenPxDistance + 0.5f, 0.f, 1.f);

	if (IS_ZERO(opacity))
		discard;
}
