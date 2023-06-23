#include "mesh_vertex_input_layout.h"

layout(binding = 0) buffer MeshTransformsBuffer
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
    const vec4 worldPos = g_Transforms[a_PerInstanceData.x] * vec4(a_Position, 1.0);
#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * worldPos;
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * worldPos;
#else
    gl_Position = g_ViewProj * worldPos;
#endif
}
