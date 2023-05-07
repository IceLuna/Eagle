#ifndef EG_SPRITES
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
    gl_Position = g_ViewProjections[gl_ViewIndex] * vec4(worldPos.xyz - a_Normal * 0.0069f, 1.f);
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * vec4(worldPos.xyz - a_Normal * 0.003f, 1.f);
#else
    gl_Position = g_ViewProj * vec4(worldPos.xyz - a_Normal * 0.006f, 1.f);
#endif
}

#else

#include "sprite_vertex_input_layout.h"

// For point lights & multi-view depth-pass
#ifdef EG_POINT_LIGHT_PASS
#extension GL_EXT_multiview : enable
layout(binding = 0) uniform ViewProjectionsBuffer
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
#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * vec4(a_WorldPosition - a_Normal * 0.01f, 1.0);
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * vec4(a_WorldPosition + a_Normal * 0.01f, 1.0);
#else
    gl_Position = g_ViewProj * vec4(a_WorldPosition - a_Normal * 0.01f, 1.0);
#endif
}

#endif
