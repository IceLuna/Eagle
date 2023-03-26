#include "pipeline_layout.h"
#include "utils.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryNormal;
layout(location = 2) out vec4 outShadingNormal;
layout(location = 3) out vec4 outEmissive;
layout(location = 4) out vec4 outMaterialData;
layout(location = 5) out int  outObjectID;

layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) in vec4 o_TintColor;
layout(location = 6) flat in uint i_PackedTextureIndices;
layout(location = 7) flat in uint i_PackedTextureIndices2;
layout(location = 8) flat in int i_EntityID;

void main()
{
    CPUMaterial cpuMaterial;
	cpuMaterial.PackedTextureIndices = i_PackedTextureIndices;
	cpuMaterial.PackedTextureIndices2 = i_PackedTextureIndices2;

    uint albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex, emissiveTextureIndex;
	UnpackTextureIndices(cpuMaterial, albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex, emissiveTextureIndex);

    vec3 geometryNormal = normalize(i_Normal);
    vec3 shadingNormal = geometryNormal;
	if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		shadingNormal = ReadTexture(normalTextureIndex, i_TexCoords).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
	}

	float metallness = ReadTexture(metallnessTextureIndex, i_TexCoords).x;
	float roughness = (roughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(roughnessTextureIndex, i_TexCoords).x : 0.5f; // Default roughness = 1.f
	roughness = max(roughness, 0.04f);
	float ao = (aoTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(aoTextureIndex, i_TexCoords).r : 1.f; // Default ao = 1.f

    outAlbedo = ReadTexture(albedoTextureIndex, i_TexCoords) * o_TintColor;
	outGeometryNormal = vec4(EncodeNormal(geometryNormal), 1.f);
	outShadingNormal = vec4(EncodeNormal(shadingNormal), 1.f);
	outEmissive = ReadTexture(emissiveTextureIndex, i_TexCoords);
	outMaterialData = vec4(metallness, roughness, ao, 1.f);
	outObjectID = i_EntityID;
}
