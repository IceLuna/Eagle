#include "skeletal_mesh_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1)
readonly buffer MeshPrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};
#endif

layout(set = 3, binding = 0)
readonly buffer MeshAnimTransformsBuffer
{
    mat4 Transforms[];
} g_MeshAnimation[];

#ifdef EG_MOTION
layout(set = 4, binding = 0)
readonly buffer PrevMeshAnimTransformsBuffer
{
    mat4 Transforms[];
} g_PrevMeshAnimation[];
#endif

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
};

#ifdef EG_JITTER
layout(set = 1, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out uint o_ObjectID;
layout(location = 7) flat out uint o_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

void main()
{
    const uint transformIndex = a_PerInstanceData.x & (EG_RECEIVES_DECALS_MASK - 1); // Get all but the highest bit
    const uint materialIndex = a_PerInstanceData.y;
    const uint objectID = a_PerInstanceData.z;
    o_ReceivesDecals = (a_PerInstanceData.x & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK ? 1u : 0u;

    const mat4 model = g_Transforms[transformIndex];
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

    gl_Position = g_ViewProjection * model * totalPosition;
    const vec3 normal = mat3(boneTransform) * a_Normal;
    const mat3 normalModel = transpose(inverse(mat3(model)));
    const vec3 worldNormal = normalize(normalModel * normal);

    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(normalModel * a_Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = materialIndex;
    o_ObjectID = objectID;

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    {
        vec4 prevTotalPosition = vec4(a_Position, 1.0);
    
        mat4 prevBoneTransform = mat4(0.f);
        for (uint i = 0; i < 4; ++i)
        {
            if (a_Weights[i] > 0.f)
            {
                const uint meshAnimIndex = a_PerInstanceData.w;
                prevBoneTransform += g_PrevMeshAnimation[nonuniformEXT(meshAnimIndex)].Transforms[a_BoneIDs[i]] * a_Weights[i];
            }
        }

        prevTotalPosition = prevBoneTransform * vec4(a_Position, 1.0);

        const mat4 prevModel = g_PrevTransforms[transformIndex];
        const vec4 prevPos = g_PrevViewProjection * prevModel * prevTotalPosition;
        o_PrevPos = prevPos.xyw;
    }
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
