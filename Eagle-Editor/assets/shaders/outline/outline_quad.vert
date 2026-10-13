#include "defines.h"

#if defined(EG_SPRITE) + defined(EG_LIT_TEXT) + defined(EG_UNLIT_TEXT) + defined(EG_BILLBOARD) != 1
#error "Exactly one quad type must be defined"
#endif

#if defined(EG_MATERIALS_REQUIRED) && (defined(EG_UNLIT_TEXT) || defined(EG_BILLBOARD))
#error "Unlit texts and billboards don't have materials"
#endif

#if defined(EG_SPRITE)
#include "sprite_vertex_input_layout.h"
#elif defined(EG_LIT_TEXT)
#include "text/text_lit_vertex_input_layout.h"
#elif defined(EG_UNLIT_TEXT)
#include "text/text_unlit_vertex_input_layout.h"
#elif defined(EG_BILLBOARD)
#include "billboard_vertex_input_layout.h"
#endif

#include "outline/outline_common.h"

// Billboards are already in view space
#ifndef EG_BILLBOARD
layout(set = EG_OUTLINE_GEOMETRY_SET, binding = 0) readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

mat4 GetWorldTransform()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK);
    return g_Transforms[transformIndex];
}
#endif

layout(location = 0) flat out uint o_StyleID;
#if defined(EG_MATERIALS_REQUIRED) || defined(EG_LIT_TEXT) || defined(EG_UNLIT_TEXT) || defined(EG_BILLBOARD)
layout(location = 1) out vec2 o_TexCoords;
#endif
#ifdef EG_MATERIALS_REQUIRED
layout(location = 2) flat out uint o_MaterialIndex;
#endif
#if defined(EG_LIT_TEXT) || defined(EG_UNLIT_TEXT) || defined(EG_BILLBOARD)
layout(location = 3) flat out uint o_TextureIndex;
#endif

void main()
{
    const uint vertexID = gl_VertexIndex % 4u;

#if defined(EG_SPRITE)
    const vec4 position = GetWorldTransform() * vec4(s_QuadVertexPosition[vertexID], 1.0);
#elif defined(EG_LIT_TEXT) || defined(EG_UNLIT_TEXT)
    const vec4 position = GetWorldTransform() * vec4(a_Position, 0.0, 1.0);
#elif defined(EG_BILLBOARD)
    const vec4 position = vec4(a_ModelViewPos, 1.0); // Already in view space
#endif
    gl_Position = g_PushData.ViewProj * position; // For billboards, `ViewProj` is actually just a `Proj`

    o_StyleID = g_PushData.StyleID;

#if defined(EG_SPRITE) && defined(EG_MATERIALS_REQUIRED)
    o_TexCoords = a_TexCoords;
#elif defined(EG_LIT_TEXT) || defined(EG_UNLIT_TEXT)
    o_TexCoords = a_TexCoords;
    o_TextureIndex = a_AtlasIndex;
#elif defined(EG_BILLBOARD)
    o_TexCoords = s_TexCoords[vertexID];
    o_TextureIndex = a_TextureIndex;
#endif

#ifdef EG_MATERIALS_REQUIRED
    o_MaterialIndex = a_MaterialIndex;
#endif
}
