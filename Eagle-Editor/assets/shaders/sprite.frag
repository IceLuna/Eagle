#include "pipeline_layout.h"
#include "utils.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterialData;

layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_PackedTextureIndices;
layout(location = 6) flat in uint i_PackedTextureIndices2;

void main()
{
    CPUMaterial cpuMaterial;
	cpuMaterial.PackedTextureIndices = i_PackedTextureIndices;
	cpuMaterial.PackedTextureIndices2 = i_PackedTextureIndices2;

    uint albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex;
	UnpackTextureIndices(cpuMaterial, albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex);

    vec3 normal;
	if (normalTextureIndex != INVALID_TEXTURE_INDEX)
	{
		normal = ReadTexture(normalTextureIndex, i_TexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(i_TBN * normal);
	}
	else
	{
	    normal = normalize(i_Normal);
	}

	float metallness = ReadTexture(metallnessTextureIndex, i_TexCoords).x;
	float roughness = (roughnessTextureIndex != INVALID_TEXTURE_INDEX) ? ReadTexture(roughnessTextureIndex, i_TexCoords).x : 0.5f; // Default roughness = 1.f
	roughness = max(roughness, 0.04f);
	float ao = (aoTextureIndex != INVALID_TEXTURE_INDEX) ? ReadTexture(aoTextureIndex, i_TexCoords).r : 1.f; // Default ao = 1.f

    outAlbedo = ReadTexture(albedoTextureIndex, i_TexCoords);
	outNormal = vec4(EncodeNormal(normal), 1.f);
	outMaterialData = vec4(metallness, roughness, ao, 1.f);
}
