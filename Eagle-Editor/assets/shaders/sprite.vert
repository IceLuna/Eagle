#include "defines.h"
#include "common_structures.h"
#include "sprite_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) out vec4 o_TintColor;
layout(location = 6) out vec3 o_EmissionIntensity;
layout(location = 7) flat out uint o_PackedTextureIndices;
layout(location = 8) flat out uint o_PackedTextureIndices2;
layout(location = 9) flat out int o_EntityID;

void main()
{
    gl_Position = g_ViewProj * vec4(a_WorldPosition, 1.0);

    CPUMaterial cpuMaterial;
	cpuMaterial.PackedTextureIndices = a_PackedTextureIndices;
	cpuMaterial.PackedTextureIndices2 = a_PackedTextureIndices2;

    uint normalTextureIndex, unused;
	UnpackTextureIndices(cpuMaterial, unused, unused, normalTextureIndex, unused, unused, unused);
    if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
        o_TBN = mat3(a_WorldTangent, a_WorldBitangent, a_WorldNormal);

    o_Normal = a_Normal;
    o_TexCoords = a_TexCoords * a_TilingFactor;
    o_TintColor = a_TintColor;
    o_EmissionIntensity = a_EmissionIntensity;
    o_PackedTextureIndices  = a_PackedTextureIndices;
    o_PackedTextureIndices2 = a_PackedTextureIndices2;
    o_EntityID = a_EntityID;
}
