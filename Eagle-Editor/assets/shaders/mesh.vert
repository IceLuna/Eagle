#include "pipeline_layout.h"
#include "mesh_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_Model;
    uint g_MaterialIndex;
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
uniform AdditionalMeshData
{
    mat4 g_ViewProjection;
};

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;

void main()
{
    gl_Position = g_ViewProjection * g_Model * vec4(a_Position, 1.0);

    ShaderMaterial material = FetchMaterial(g_MaterialIndex);
    if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
    {
        vec3 tangent = normalize(vec3(g_Model * vec4(a_Tangent, 0.0)));
        vec3 normal = normalize(vec3(g_Model * vec4(a_Normal, 0.0)));
        tangent = normalize(tangent - normal * dot(tangent, normal));
        vec3 bitangent = normalize(cross(normal, tangent));
        o_TBN = mat3(tangent, bitangent, normal);
    }
    else
    {
        o_Normal = mat3(transpose(inverse(g_Model))) * a_Normal;
    }
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = g_MaterialIndex;
}
