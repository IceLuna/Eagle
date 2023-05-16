#include "pipeline_layout.h"
#include "utils.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryNormal;
layout(location = 2) out vec4 outShadingNormal;
layout(location = 3) out vec4 outEmissive;
layout(location = 4) out vec2 outMaterialData;
layout(location = 5) out int  outObjectID;

layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) in vec4 o_TintColor;
layout(location = 6) in vec3 o_EmissionIntensity;
layout(location = 7) flat in uint i_PackedTextureIndices;
layout(location = 8) flat in uint i_PackedTextureIndices2;
layout(location = 9) flat in int i_EntityID;

void main()
{
    uint albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex, emissiveTextureIndex;

	{
	    CPUMaterial cpuMaterial;
		cpuMaterial.PackedTextureIndices = i_PackedTextureIndices;
		cpuMaterial.PackedTextureIndices2 = i_PackedTextureIndices2;

		UnpackTextureIndices(cpuMaterial, albedoTextureIndex, metallnessTextureIndex, normalTextureIndex, roughnessTextureIndex, aoTextureIndex, emissiveTextureIndex);
	}

    vec3 geometryNormal = normalize(i_Normal);
    vec3 shadingNormal = geometryNormal;
	if (normalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		shadingNormal = ReadTexture(normalTextureIndex, i_TexCoords).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
	}

	const float metallness = ReadTexture(metallnessTextureIndex, i_TexCoords).x;
	float roughness = (roughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(roughnessTextureIndex, i_TexCoords).x : EG_DEFAULT_ROUGHNESS;
	roughness = max(roughness, EG_MIN_ROUGHNESS);
	const float ao = (aoTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(aoTextureIndex, i_TexCoords).r : EG_DEFAULT_AO;

    outAlbedo = vec4(ReadTexture(albedoTextureIndex, i_TexCoords).rgb * o_TintColor.rgb, roughness);
	outGeometryNormal = vec4(EncodeNormal(geometryNormal), 1.f);
	outShadingNormal = vec4(EncodeNormal(shadingNormal), 1.f);
	outEmissive = ReadTexture(emissiveTextureIndex, i_TexCoords) * vec4(o_EmissionIntensity, 1.0);
	outMaterialData = vec2(metallness, ao);
	outObjectID = i_EntityID;
}
