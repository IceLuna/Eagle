#extension GL_EXT_nonuniform_qualifier : enable
#include "skeletal_mesh_vertex_input_layout.h"

#ifdef EG_MATERIALS_REQUIRED
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

layout(set = 3, binding = 0)
readonly buffer MeshAnimTransformsBuffer
{
    mat4 Transforms[];
} g_MeshAnimation[];

#ifdef EG_MATERIALS_REQUIRED
layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_MaterialIndex;
#endif

void main()
{
    vec4 totalPosition = vec4(a_Position, 1.0);
    mat4 boneTransform = mat4(0.f);
    for (uint i = 0; i < 4; ++i)
    {
        if (a_Weights[i] > 0.f)
        {
            const uint meshAnimIndex = a_PerInstanceData.w;
            boneTransform += g_MeshAnimation[nonuniformEXT(meshAnimIndex)].Transforms[a_BoneIDs[i]] * a_Weights[i];
        }
    }

    totalPosition = boneTransform * vec4(a_Position, 1.0);

    const vec4 worldPos = g_Transforms[a_PerInstanceData.x] * totalPosition;
#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[gl_ViewIndex] * worldPos;
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * worldPos;
#else
    gl_Position = g_ViewProj * worldPos;
#endif

#ifdef EG_MATERIALS_REQUIRED
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = a_PerInstanceData.y;
#endif
}
