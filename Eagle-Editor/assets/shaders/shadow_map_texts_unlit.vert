#include "text_unlit_vertex_input_layout.h"

layout(binding = 0) buffer SpriteTransformsBuffer
{
    mat4 g_Transforms[];
};

// For point lights & multi-view depth-pass
#ifdef EG_POINT_LIGHT_PASS
#extension GL_EXT_multiview : enable
layout(binding = 1) uniform ViewProjectionsBuffer
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

void main()
{
    const uint vertexID = gl_VertexIndex % 4u;
    const mat4 model = g_Transforms[a_TransformIndex];
    const vec4 worldPos = model * vec4(a_Position, 1.f);

    o_TexCoords  = a_TexCoords;
    o_AtlasIndex = a_AtlasIndex;

#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * worldPos;
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * worldPos;
#else
    gl_Position = g_ViewProj * worldPos;
#endif
}
