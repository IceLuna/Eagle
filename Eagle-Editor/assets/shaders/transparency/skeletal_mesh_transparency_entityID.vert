#include "defines.h"
#include "skeletal_mesh_vertex_input_layout.h"
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(set = 5, binding = 0)
readonly buffer MeshAnimTransformsBuffer
{
    mat4 Transforms[];
} g_MeshAnimation[];

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

layout(location = 0) flat out int o_ObjectID;

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

    const mat4 model = g_Transforms[a_PerInstanceData.x];
    gl_Position = g_ViewProjection * model * totalPosition;
 
    o_ObjectID = int(a_PerInstanceData.z);
}
