#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"

#ifdef EG_MATERIALS_REQUIRED
#include "pipeline_layout.h"
const uint s_Set = 3;
#else
const uint s_Set = 1;
#endif

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_AtlasIndex;
#ifdef EG_MATERIALS_REQUIRED
layout(location = 2) flat in uint i_MaterialIndex;
#endif

#ifdef EG_TRANSLUCENT
layout(location = 0) out vec4 o_Color;
#ifdef EG_OUTPUT_DEPTH
layout(location = 1) out float o_Depth;
#endif
#endif

layout(set = s_Set, binding = 0) uniform sampler2D g_FontAtlases[];

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
#ifdef EG_MATERIALS_REQUIRED
    vec2 uv = i_TexCoords;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
#endif

#ifdef EG_MASKED
	if (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD)
	{
		discard;
		return;
	}
#endif

	const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords).rgb;
    const float sd = median(msd.r, msd.g, msd.b);
    const float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    const float opacity = clamp(screenPxDistance + 0.5f, 0.f, 1.f);

	if (IS_ZERO(opacity))
		discard;

#ifdef EG_TRANSLUCENT
	o_Color = vec4(material.Albedo * (1.f - material.Opacity), 1.f);
#ifdef EG_OUTPUT_DEPTH
	o_Depth = gl_FragCoord.z;
#endif
#endif
}
