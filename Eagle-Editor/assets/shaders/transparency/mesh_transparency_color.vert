#include "defines.h"
#include "mesh_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(set = 5, binding = 1)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) out vec3 o_WorldPos;
layout(location = 4) out mat3 o_TBN;

void main()
{
    const uint transformIndex = GetTransformIndex();
    const uint materialIndex = GetMaterialIndex();

    const mat4 model = g_Transforms[transformIndex];
    const vec4 worldPos = model * vec4(a_Position, 1);
    gl_Position = g_ViewProjection * worldPos;

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * a_Normal);

    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(normalModel * a_Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_WorldPos = worldPos.xyz;
    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = materialIndex;
}
