#include "defines.h"
#include "skeletal_mesh_vertex_input_layout.h"
#include "material_pipeline_layout.h"
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
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

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) out vec3 o_WorldPos;
layout(location = 4) out mat3 o_TBN;

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
    
    o_WorldPos = vec3(model * boneTransform * totalPosition);
    
    ShaderMaterial material = FetchMaterial(a_PerInstanceData.y);
    if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
    {
        vec3 tangent = normalize(vec3(model * boneTransform * vec4(a_Tangent, 0.0)));
        vec3 normal = normalize(vec3(model * boneTransform * vec4(a_Normal, 0.0)));
        tangent = normalize(tangent - normal * dot(tangent, normal));
        vec3 bitangent = normalize(cross(normal, tangent));
        o_TBN = mat3(tangent, bitangent, normal);
    }

    o_Normal = mat3(transpose(inverse(model * boneTransform))) * a_Normal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = a_PerInstanceData.y;
}
