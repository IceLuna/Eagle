#include "defines.h"
#include "sprite_vertex_input_layout.h"

#ifndef EG_DEPTH_ONLY
#define EG_NO_TEXTURES
#include "pipeline_layout.h"
#endif

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
};

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

#ifdef EG_JITTER
layout(set = 1, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

#ifndef EG_DEPTH_ONLY

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out int o_EntityID;
layout(location = 7) flat out uint o_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

#endif // #ifndef EG_DEPTH_ONLY

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);

#ifndef EG_DEPTH_ONLY
    const uint materialIndex  = a_MaterialIndex;
    o_ReceivesDecals = (a_TransformIndex & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK ? 1u : 0u;

    const CPUMaterial material = g_Materials[materialIndex];
    bool unused;
    const uint normalTextureIndex = Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, unused);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) >= 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_TexCoords = a_TexCoords * material.TilingFactor;
    o_MaterialIndex  = materialIndex;
    o_EntityID = a_EntityID;
#endif // #ifndef EG_DEPTH_ONLY

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[transformIndex];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(s_QuadVertexPosition[vertexID], 1.f);
    o_PrevPos = prevPos.xyw;
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
