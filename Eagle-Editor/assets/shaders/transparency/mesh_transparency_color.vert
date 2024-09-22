#include "defines.h"
#include "mesh_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

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
    const uint transformIndex = a_PerInstanceData.x & (EG_RECEIVES_DECALS_MASK - 1); // Get all but the highest bit
    const uint materialIndex = a_PerInstanceData.y;

    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);
    
    o_WorldPos = vec3(model * vec4(a_Position, 1.0));
    
    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(vec3(model * vec4(a_Tangent, 0.0)));
        vec3 normal = normalize(vec3(model * vec4(a_Normal, 0.0)));
        tangent = normalize(tangent - normal * dot(tangent, normal));
        vec3 bitangent = normalize(cross(normal, tangent));
        o_TBN = mat3(tangent, bitangent, normal);
    }

    o_Normal = mat3(transpose(inverse(model))) * a_Normal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = materialIndex;
}
