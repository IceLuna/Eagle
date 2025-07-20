#include "defines.h"
#include "text/text_lit_vertex_input_layout.h"

#ifdef EG_MATERIALS_REQUIRED
const uint s_Set = 1;
#else
const uint s_Set = 0;
#endif

layout(set = s_Set, binding = 0) readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

// For point lights & multi-view depth-pass
#ifdef EG_POINT_LIGHT_PASS
#extension GL_EXT_multiview : enable
layout(set = s_Set, binding = 1) uniform ViewProjectionsBuffer
{
    mat4 g_ViewProjections[6];
};
#endif

#ifndef EG_POINT_LIGHT_PASS
layout(push_constant) uniform PushData
{
    mat4 g_ViewProj;
};
#endif

layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_AtlasIndex;
#ifdef EG_MATERIALS_REQUIRED
layout(location = 2) flat out uint o_MaterialIndex;
#endif

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const uint vertexID = gl_VertexIndex % 4u;
    const mat4 model = g_Transforms[transformIndex];
    const vec4 worldPos = model * vec4(a_Position, 0.f, 1.f);

    o_TexCoords = a_TexCoords;
    o_AtlasIndex = a_AtlasIndex;
#ifdef EG_MATERIALS_REQUIRED
    o_MaterialIndex = a_MaterialIndex;
#endif

#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * worldPos;
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * worldPos;
#else
    gl_Position = g_ViewProj * worldPos;
#endif
}
