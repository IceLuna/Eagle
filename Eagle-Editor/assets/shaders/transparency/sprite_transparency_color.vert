#include "defines.h"
#include "sprite_vertex_input_layout.h"
#include "material_pipeline_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) out vec3 o_WorldPos;
layout(location = 4) out mat3 o_TBN;


void main()
{
    const uint vertexID = gl_VertexIndex % 4u;
    const uint materialIndex  = a_MaterialIndex;
    const uint transformIndex = a_TransformIndex;

    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);
    
    o_WorldPos = vec3(model * vec4(s_QuadVertexPosition[vertexID], 1.f));

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) >= 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    const CPUMaterial material = g_Materials[materialIndex];
    uint normalTextureIndex, unused;
	UnpackTextureIndices(material, unused, unused, normalTextureIndex, unused, unused, unused, unused, unused);
    if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_TexCoords = a_TexCoords;
    o_MaterialIndex  = materialIndex;
}
