#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "outline/outline_common.h"

#if defined(EG_BILLBOARD) && !defined(EG_MATERIALS_REQUIRED)
#define EG_NO_MATERIALS // Billboards only need textures
#endif
#if defined(EG_MATERIALS_REQUIRED) || defined(EG_BILLBOARD)
#include "pipeline_layout.h"
#endif

layout(location = 0) flat in uint i_StyleID;
#if defined(EG_MATERIALS_REQUIRED) || defined(EG_TEXT) || defined(EG_BILLBOARD)
layout(location = 1) in vec2 i_TexCoords;
#endif
#ifdef EG_MATERIALS_REQUIRED
layout(location = 2) flat in uint i_MaterialIndex;
#endif
#if defined(EG_TEXT) || defined(EG_BILLBOARD)
layout(location = 3) flat in uint i_TextureIndex;
#endif

layout(set = EG_OUTLINE_PASS_SET, binding = 0) uniform sampler2D g_SceneDepth;
#ifdef EG_TEXT
layout(set = EG_OUTLINE_PASS_SET, binding = 1) uniform sampler2D g_FontAtlases[];
#endif

layout(location = 0) out uint o_Mask;

#ifdef EG_TEXT
float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}
#endif

void main()
{
    const float depth = gl_FragCoord.z;
    const float depthWidth = fwidth(depth);
    bool bDiscard = false;

#ifdef EG_TEXT
    {
        const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_TextureIndex)], i_TexCoords).rgb;
        const float sd = Median(msd.r, msd.g, msd.b);

        const float pxRange = 2.0;
        const vec2 unitRange = vec2(pxRange) / vec2(textureSize(g_FontAtlases[nonuniformEXT(i_TextureIndex)], 0));
        const vec2 screenTexSize = vec2(1.0) / fwidth(i_TexCoords);
        const float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);
        const float opacity = clamp(screenPxRange * (sd - 0.5) + 0.5, 0.0, 1.0);

        bDiscard = bDiscard || (opacity < 0.5);
    }
#endif

#ifdef EG_BILLBOARD
    {
        const float alpha = ReadTexture(i_TextureIndex, i_TexCoords).a;
        bDiscard = bDiscard || (alpha < 0.9);
    }
#endif

#ifdef EG_MATERIALS_REQUIRED
    {
        vec2 uv = i_TexCoords;
        const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
        bDiscard = bDiscard || (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD);
    }
#endif

    if (bDiscard)
    {
        discard;
    }

    // The scene depth might be rendered with TAA jitter while the mask isn't (so that outlines don't wobble).
    // So the depth slope is used as a tolerance. Plus a small relative bias to handle precision issues
    const float sceneDepth = texelFetch(g_SceneDepth, ivec2(gl_FragCoord.xy), 0).x;
    const float tolerance = depthWidth + abs(depth) * 0.001;
#ifdef EG_REVERSED_DEPTH
    const bool bVisible = (depth + tolerance) >= sceneDepth;
#else
    const bool bVisible = (depth - tolerance) <= sceneDepth;
#endif

    o_Mask = (i_StyleID & EG_OUTLINE_STYLE_MASK) | (bVisible ? EG_OUTLINE_VISIBLE_BIT : 0u);
}
