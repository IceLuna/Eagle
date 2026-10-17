#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "text/text_effects.h"

layout(location = 0) in vec3 i_Color;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in uint i_AtlasIndex;
layout(location = 3) flat in float i_Opacity;
#ifndef EG_NO_OBJECT_ID
layout(location = 4) flat in int i_EntityID;
#endif
layout(location = 5) flat in uint i_EffectsIndex;

layout(location = 0) out vec4 outColor;
#ifndef EG_NO_OBJECT_ID
layout(location = 1) out int  outObjectID;
#endif

layout(set = 0, binding = 0) uniform sampler2D g_FontAtlases[];

layout(set = 1, binding = 0) readonly buffer EffectsBuffer
{
    TextEffects g_Effects[];
};

void main()
{
    const TextEffects effects = g_Effects[i_EffectsIndex & ~EG_TEXT_SHADOW_MASK];
    const vec4 mtsdf = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);
    const vec2 atlasSize = vec2(textureSize(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], 0));

    vec4 color;
    if ((i_EffectsIndex & EG_TEXT_SHADOW_MASK) != 0u)
        color = vec4(effects.ShadowColor.rgb, TextEffects_Shadow(mtsdf, atlasSize, i_TexCoords, effects));
    else
        color = TextEffects_Shade(mtsdf, atlasSize, i_TexCoords, i_Color, effects);
    color.a *= i_Opacity;

    if (IS_ZERO(color.a))
        discard;

    outColor = color;
#ifndef EG_NO_OBJECT_ID
    outObjectID = i_EntityID;
#endif
}
