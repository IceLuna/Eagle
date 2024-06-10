#ifndef EG_PIPELINE_LAYOUT
#define EG_PIPELINE_LAYOUT

#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "common_structures.h"
#include "utils.h"

#ifndef EG_NO_TEXTURES
layout(set = EG_TEXTURES_SET, binding = EG_BINDING_TEXTURES) uniform sampler2D g_Textures[];

vec4 ReadTexture(uint index, vec2 uv)
{
	return texture(g_Textures[nonuniformEXT(index)], uv);
}
#endif

#ifndef EG_NO_MATERIALS

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MATERIALS)
readonly buffer Materials
{
	CPUMaterial g_Materials[];
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_RAW_MATERIALS)
readonly buffer RawMaterials
{
	float g_MaterialRawValues[];
};


uint FetchMaterialNormalTextureIndex(uint index)
{
	bool unused;
	CPUMaterial material = g_Materials[index];
	return Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, unused);
}

#ifndef EG_NO_TEXTURES
ShaderMaterial FetchMaterial(uint index, inout vec2 uv)
{
	ShaderMaterial result;
	CPUMaterial material = g_Materials[index];

	result.TintColor = material.TintColor;
	result.EmissiveIntensity = material.EmissiveIntensity;
	result.TilingFactor = material.TilingFactor;
	uv *= material.TilingFactor;

	bool bRawValue;

	uint albedoIndex = Material_GetIndex(material.PackedIndices, AlbedoIndexMask, AlbedoIndexOffset, bRawValue);
	if (albedoIndex != EG_INVALID_INDEX)
	{
		result.Albedo = bRawValue ?
			vec3(g_MaterialRawValues[nonuniformEXT(albedoIndex)], g_MaterialRawValues[nonuniformEXT(albedoIndex + 1)], g_MaterialRawValues[nonuniformEXT(albedoIndex + 2)]) :
			ReadTexture(albedoIndex, uv).rgb;
	}
	else
		result.Albedo = vec3(0.f);

	uint metalnessIndex = Material_GetIndex(material.PackedIndices, MetalnessIndexMask, MetalnessIndexOffset, bRawValue);
	if (metalnessIndex != EG_INVALID_INDEX)
		result.Metalness = bRawValue ? g_MaterialRawValues[nonuniformEXT(metalnessIndex)] : ReadTexture(metalnessIndex, uv).x;
	else
		result.Metalness = 0.f;

	result.NormalTextureIndex = Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, bRawValue);

	uint roughnessIndex = Material_GetIndex(material.PackedIndices2, RoughnessIndexMask, RoughnessIndexOffset, bRawValue);
	if (roughnessIndex != EG_INVALID_INDEX)
	{
		result.Roughness = bRawValue ? g_MaterialRawValues[nonuniformEXT(roughnessIndex)] : ReadTexture(roughnessIndex, uv).x;
		result.Roughness = max(result.Roughness, EG_MIN_ROUGHNESS);
	}
	else
		result.Roughness = EG_DEFAULT_ROUGHNESS;

	uint aoIndex = Material_GetIndex(material.PackedIndices3, AOIndexMask, AOIndexOffset, bRawValue);
	if (aoIndex != EG_INVALID_INDEX)
		result.AO = bRawValue ? g_MaterialRawValues[nonuniformEXT(aoIndex)] : ReadTexture(aoIndex, uv).x;
	else
		result.AO = EG_DEFAULT_AO;

	uint emissiveIndex = Material_GetIndex(material.PackedIndices3, EmissiveIndexMask, EmissiveIndexOffset, bRawValue);
	if (emissiveIndex != EG_INVALID_INDEX)
	{
		result.Emissive = bRawValue ?
			vec3(g_MaterialRawValues[nonuniformEXT(emissiveIndex)], g_MaterialRawValues[nonuniformEXT(emissiveIndex + 1)], g_MaterialRawValues[nonuniformEXT(emissiveIndex + 2)]) :
			ReadTexture(emissiveIndex, uv).rgb;
	}
	else
		result.Emissive = vec3(0.f);

	uint opacityIndex = Material_GetIndex(material.PackedIndices4, OpacityIndexMask, OpacityIndexOffset, bRawValue);
	if (opacityIndex != EG_INVALID_INDEX)
		result.Opacity = bRawValue ? g_MaterialRawValues[nonuniformEXT(opacityIndex)] : ReadTexture(opacityIndex, uv).x;
	else
		result.Opacity = 0.5f;

	uint opacityMaskIndex = Material_GetIndex(material.PackedIndices4, OpacityMaskIndexMask, OpacityMaskIndexOffset, bRawValue);
	if (opacityMaskIndex != EG_INVALID_INDEX)
		result.OpacityMask = bRawValue ? g_MaterialRawValues[nonuniformEXT(opacityMaskIndex)] : ReadTexture(opacityMaskIndex, uv).x;
	else
		result.OpacityMask = 1.f;

	return result;
}
#endif // #ifndef EG_NO_TEXTURES
#endif // #ifndef EG_NO_MATERIALS

#endif
