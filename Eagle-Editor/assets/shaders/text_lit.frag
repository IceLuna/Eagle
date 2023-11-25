#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"

layout(location = 0) flat in vec4 i_AlbedoRoughness;
layout(location = 1) flat in vec4 i_EmissiveMetallness;
layout(location = 2) in vec3 i_Normal;
layout(location = 3) flat in int i_EntityID;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_AtlasIndex;
layout(location = 6) flat in float i_AO;
layout(location = 7) flat in float i_OpacityMask;
#ifdef EG_MOTION
layout(location = 8) in vec3 i_CurPos;
layout(location = 9) in vec3 i_PrevPos;
#endif

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryShadingNormals;
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out vec2 outMaterialData;
layout(location = 4) out int  outObjectID;
#ifdef EG_MOTION
layout(location = 5) out vec2 outMotion;
#endif

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[];

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
	//const vec4 bgColor = vec4(i_Color, 1.0);
	//const vec4 fgColor = vec4(i_Color, 1.0);
    //outColor = mix(bgColor, fgColor, opacity);

#ifdef EG_MASKED
    if (i_OpacityMask < EG_OPACITY_MASK_THRESHOLD)
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
    {
		discard;
        return;
    }

    const vec2 packedNormal = EncodeNormal(normalize(i_Normal));

    outAlbedo = i_AlbedoRoughness;
    outGeometryShadingNormals = vec4(packedNormal, packedNormal);
    outEmissive = vec4(i_EmissiveMetallness.rgb, 1.f);
    outMaterialData = vec2(i_EmissiveMetallness.a, i_AO);
    outObjectID = i_EntityID;

    // TODO: Pack to outEmissive.a since it's not used anyway
#ifdef EG_MOTION
	outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
