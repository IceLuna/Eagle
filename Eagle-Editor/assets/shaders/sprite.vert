#include "defines.h"
#include "common_structures.h"
#include "sprite_vertex_input_layout.h"
#include "material_pipeline_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1)
buffer MeshPrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};
#endif

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out int o_EntityID;
#ifdef EG_MOTION
layout(location = 7) out vec3 o_CurPos;
layout(location = 8) out vec3 o_PrevPos;
#endif

void main()
{
    const uint vertexID = gl_VertexIndex % 4u;
    const bool bInvert = (gl_VertexIndex % 8u) >= 4;

    const mat4 model = g_Transforms[a_TransformIndex];

    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);

	const CPUMaterial material = g_Materials[a_MaterialIndex];
    uint normalTextureIndex, unused;
	UnpackTextureIndices(material, unused, unused, normalTextureIndex, unused, unused, unused);
    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_TexCoords = a_TexCoords * material.TilingFactor;
    o_MaterialIndex  = a_MaterialIndex;
    o_EntityID = a_EntityID;

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[a_TransformIndex];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(s_QuadVertexPosition[vertexID], 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
