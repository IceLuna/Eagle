#include "text/text_lit_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
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
#endif

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) flat out int o_EntityID;
layout(location = 5) out vec2 o_TexCoords;
layout(location = 6) flat out uint o_AtlasIndex;
layout(location = 7) flat out uint o_MaterialIndex;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 0.f, 1.0);

    const uint materialIndex  = a_MaterialIndex;
    const CPUMaterial material = g_Materials[materialIndex];
    bool unused;
    const uint normalTextureIndex = Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, unused);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) < 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_TexCoords = a_TexCoords;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
    o_MaterialIndex = materialIndex;

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[a_TransformIndex];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(a_Position, 0.f, 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
