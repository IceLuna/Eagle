#include "defines.h"
#include "text/text_lit_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec3 o_WorldPos;
layout(location = 2) flat out uint o_AtlasIndex;
layout(location = 3) flat out uint o_MaterialIndex;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) out mat3 o_TBN;

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 0.f, 1.0);
    
    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) < 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    const uint materialIndex  = a_MaterialIndex;
    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_WorldPos = vec3(model * vec4(a_Position, 0.f, 1.0));
    o_AtlasIndex = a_AtlasIndex;
    o_MaterialIndex = materialIndex;
    o_TexCoords = a_TexCoords;
}
