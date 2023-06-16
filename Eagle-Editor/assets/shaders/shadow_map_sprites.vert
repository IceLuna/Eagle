#include "sprite_vertex_input_layout.h"

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

void main()
{
    const uint vertexID = gl_VertexIndex % 4u;
    const uint transformIndex = gl_VertexIndex / 8u;

    const mat4 model = g_Transforms[transformIndex];
    const vec4 worldPos = model * vec4(s_QuadVertexPosition[vertexID], 1.f);
    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalModel * s_Normal;

    const bool bInvert = (gl_VertexIndex % 8u) >= 4;
    if (bInvert)
        worldNormal = -worldNormal;

#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * vec4(worldPos.xyz - worldNormal * 0.01f, 1.0);
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * vec4(worldPos.xyz + worldNormal * 0.01f, 1.0);
#else
    gl_Position = g_ViewProj * vec4(worldPos.xyz - worldNormal * 0.01f, 1.0);
#endif
}
