#include "defines.h"
#include "common_structures.h"

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec3  a_ModelNormal;
layout(location = 3) in vec3  a_Tangent;
layout(location = 4) in vec2  a_TexCoords;
layout(location = 5) in int   a_EntityID;

// Material data
layout(location = 6) in vec4  a_TintColor;
layout(location = 7) in float a_TilingFactor;
layout(location = 8) in float a_Shininess;
layout(location = 9) in uint a_PackedTextureIndices;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_PackedTextureIndices;

void main()
{
    gl_Position = g_ViewProj * vec4(a_Position, 1.0);

    uint normalTextureIndex, unused;
	UnpackTextureIndices(a_PackedTextureIndices, unused, unused, normalTextureIndex);
    if (normalTextureIndex != INVALID_TEXTURE_INDEX)
    {
        vec3 T = normalize(a_Tangent - (a_ModelNormal * dot(a_Tangent, a_ModelNormal)));
        vec3 bitangent = normalize(cross(a_ModelNormal, T));
        o_TBN = transpose(mat3(T, bitangent, a_ModelNormal));
    }

    o_Normal = a_Normal;
    o_TexCoords = a_TexCoords;
    o_PackedTextureIndices = a_PackedTextureIndices;
}
