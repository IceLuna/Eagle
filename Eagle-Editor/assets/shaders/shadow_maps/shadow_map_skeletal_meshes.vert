#extension GL_EXT_nonuniform_qualifier : enable
#include "defines.h"
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
    const uint transformIndex = a_PerInstanceData.x & (EG_RECEIVES_DECALS_MASK - 1); // Get all but the highest bit

    vec4 totalPosition = vec4(a_Position, 1.0);
    mat4 boneTransform = mat4(0.f);
    for (uint i = 0; i < 4; ++i)
    {
        const float weight = GetWeight(i);
        if (weight > 0.f)
        {
            boneTransform += g_MeshAnimation[nonuniformEXT(transformIndex)].Transforms[GetBoneID(i)] * weight;
        }
    }

    totalPosition = boneTransform * vec4(a_Position, 1.0);

    const vec4 worldPos = g_Transforms[transformIndex] * totalPosition;
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
