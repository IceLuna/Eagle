#include "defines.h"
#include "text/text_lit_vertex_input_layout.h"

#ifndef EG_DEPTH_ONLY
#define EG_NO_TEXTURES
#include "pipeline_layout.h"
#endif

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1)
readonly buffer PrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 2) uniform CameraData
{
    mat4 View;
    mat4 InvViewProj;
    mat4 ViewProj;
    mat4 PrevViewProj;
    mat4 Proj;
    mat4 InvProj;
    mat4 PrevProj;
    mat4 PrevView;
    mat4 ViewProjUnjittered;
    mat4 PrevViewProjUnjittered;
} g_Camera;
#endif

#ifndef EG_DEPTH_ONLY

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) flat out int o_EntityID;
layout(location = 5) out vec2 o_TexCoords;
layout(location = 6) flat out uint o_AtlasIndex;
layout(location = 7) flat out uint o_MaterialIndex;
layout(location = 8) flat out uint o_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 9) out vec3 o_CurPos;
layout(location = 10) out vec3 o_PrevPos;
#endif
layout(location = 11) out vec2 o_MaterialUV;

#endif // #ifndef EG_DEPTH_ONLY

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit

    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 0.f, 1.0);

#ifndef EG_DEPTH_ONLY
    const uint materialIndex  = a_MaterialIndex;
    const CPUMaterial material = g_Materials[materialIndex];
    bool unused;
    const uint normalTextureIndex = Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, unused);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * s_Normal);

    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialUV = GetTextMaterialUV();
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
    o_MaterialIndex = materialIndex;
    o_ReceivesDecals = (a_TransformIndex & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK ? 1u : 0u;
#endif // #ifndef EG_DEPTH_ONLY

#ifdef EG_MOTION
    const vec4 curPos = g_Camera.ViewProjUnjittered * model * vec4(a_Position, 0.f, 1.f);
    o_CurPos = curPos.xyw;

    const mat4 prevModel = g_PrevTransforms[transformIndex];
    const vec4 prevPos = g_Camera.PrevViewProjUnjittered * prevModel * vec4(a_Position, 0.f, 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
