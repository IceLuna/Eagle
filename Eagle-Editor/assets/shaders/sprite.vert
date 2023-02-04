#include "defines.h"
#include "common_structures.h"

layout(location = 0) in vec3  a_WorldPosition;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec3  a_WorldTangent;
layout(location = 3) in vec3  a_WorldBitangent;
layout(location = 4) in vec3  a_WorldNormal;
layout(location = 5) in vec2  a_TexCoords;
layout(location = 6) in int   a_EntityID;

// Material data
layout(location = 7)  in vec4  a_TintColor;
layout(location = 8)  in float a_TilingFactor;
layout(location = 9)  in uint  a_PackedTextureIndices;
layout(location = 10) in uint  a_PackedTextureIndices2;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_PackedTextureIndices;
layout(location = 6) flat out uint o_PackedTextureIndices2;

void main()
{
    gl_Position = g_ViewProj * vec4(a_WorldPosition, 1.0);

    CPUMaterial cpuMaterial;
	cpuMaterial.PackedTextureIndices = a_PackedTextureIndices;
	cpuMaterial.PackedTextureIndices2 = a_PackedTextureIndices2;

    uint normalTextureIndex, unused;
	UnpackTextureIndices(cpuMaterial, unused, unused, normalTextureIndex, unused, unused);
    if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
        o_TBN = mat3(a_WorldTangent, a_WorldBitangent, a_WorldNormal);

    o_Normal = a_Normal;
    o_TexCoords = a_TexCoords;
    o_PackedTextureIndices  = a_PackedTextureIndices;
    o_PackedTextureIndices2 = a_PackedTextureIndices2;
}
