#include "mesh_vertex_input_layout.h"

#ifdef EG_MASKED
const uint s_Set = 1;
#else
const uint s_Set = 0;
#endif

layout(set = s_Set, binding = 0) readonly buffer MeshTransformsBuffer
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

#ifdef EG_MASKED
layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_MaterialIndex;
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

#ifdef EG_MASKED
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = a_PerInstanceData.y;
#endif
}
