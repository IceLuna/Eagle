#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"

layout(location = 0) in vec4 i_AlbedoRoughness;
layout(location = 1) in vec4 i_EmissiveMetallness;
layout(location = 2) in vec3 i_Normal;
layout(location = 3) flat in int i_EntityID;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_AtlasIndex;
layout(location = 6) in float o_AO;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryNormal;
layout(location = 2) out vec4 outShadingNormal;
layout(location = 3) out vec4 outEmissive;
layout(location = 4) out vec2 outMaterialData;
layout(location = 5) out int  outObjectID;

layout(set = 0, binding = 0) uniform sampler2D g_FontAtlases[EG_MAX_TEXTURES];

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

	const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords).rgb;
    const float sd = median(msd.r, msd.g, msd.b);
    const float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    const float opacity = clamp(screenPxDistance + 0.5f, 0.f, 1.f);

	if (opacity != 1.f)
		discard;

    const vec4 normal = vec4(EncodeNormal(i_Normal), 1.f);

    outAlbedo = i_AlbedoRoughness;
    outGeometryNormal = normal;
    outShadingNormal = normal;
    outEmissive = vec4(i_EmissiveMetallness.rgb, 1.f);
    outMaterialData = vec2(i_EmissiveMetallness.a, o_AO);
    outObjectID = i_EntityID;
}
