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
	const CPUMaterial material = g_Materials[index];

	result.TilingFactor = material.TilingFactor;
	uv *= material.TilingFactor;

	bool bRawValue;

	uint albedoIndex = Material_GetIndex(material.PackedIndices, AlbedoIndexMask, AlbedoIndexOffset, bRawValue);
	if (albedoIndex != EG_INVALID_INDEX)
	{
		result.Albedo = bRawValue ?
			vec3(g_MaterialRawValues[albedoIndex], g_MaterialRawValues[albedoIndex + 1], g_MaterialRawValues[albedoIndex + 2]) :
			ReadTexture(albedoIndex, uv).rgb;
	}
	else
	{
		result.Albedo = vec3(0.f);
	}

	uint metalnessIndex = Material_GetIndex(material.PackedIndices, MetalnessIndexMask, MetalnessIndexOffset, bRawValue);
	if (metalnessIndex != EG_INVALID_INDEX)
	{
		if (bRawValue)
		{
			result.Metalness = g_MaterialRawValues[metalnessIndex];
		}
		else
		{
			bool bUnused;
			const uint channel = Material_GetIndex(material.PackedIndices, MetalnessTextureChannelMask, MetalnessTextureChannelOffset, bUnused);
			const vec4 val = ReadTexture(metalnessIndex, uv);
			result.Metalness = val[channel];
		}
	}
	else
	{
		result.Metalness = 0.f;
	}

	result.NormalTextureIndex = Material_GetIndex(material.PackedIndices2, NormalIndexMask, NormalIndexOffset, bRawValue);

	uint roughnessIndex = Material_GetIndex(material.PackedIndices2, RoughnessIndexMask, RoughnessIndexOffset, bRawValue);
	if (roughnessIndex != EG_INVALID_INDEX)
	{
		if (bRawValue)
		{
			result.Roughness = g_MaterialRawValues[roughnessIndex];
		}
		else
		{
			bool bUnused;
			const uint channel = Material_GetIndex(material.PackedIndices, RoughnessTextureChannelMask, RoughnessTextureChannelOffset, bUnused);
			const vec4 val = ReadTexture(roughnessIndex, uv);
			result.Roughness = val[channel];
		}
		result.Roughness = max(result.Roughness, EG_MIN_ROUGHNESS);
	}
	else
	{
		result.Roughness = EG_DEFAULT_ROUGHNESS;
	}

	uint aoIndex = Material_GetIndex(material.PackedIndices3, AOIndexMask, AOIndexOffset, bRawValue);
	if (aoIndex != EG_INVALID_INDEX)
	{
		if (bRawValue)
		{
			result.AO = g_MaterialRawValues[aoIndex];
		}
		else
		{
			bool bUnused;
			const uint channel = Material_GetIndex(material.PackedIndices2, AOTextureChannelMask, AOTextureChannelOffset, bUnused);
			const vec4 val = ReadTexture(aoIndex, uv);
			result.AO = val[channel];
		}
	}
	else
	{
		result.AO = EG_DEFAULT_AO;
	}

	uint emissiveIndex = Material_GetIndex(material.PackedIndices3, EmissiveIndexMask, EmissiveIndexOffset, bRawValue);
	if (emissiveIndex != EG_INVALID_INDEX)
	{
		result.Emissive = bRawValue ?
			vec3(g_MaterialRawValues[emissiveIndex], g_MaterialRawValues[emissiveIndex + 1], g_MaterialRawValues[emissiveIndex + 2]) :
			ReadTexture(emissiveIndex, uv).rgb;
	}
	else
	{
		result.Emissive = vec3(0.f);
	}

	uint opacityIndex = Material_GetIndex(material.PackedIndices4, OpacityIndexMask, OpacityIndexOffset, bRawValue);
	if (opacityIndex != EG_INVALID_INDEX)
	{
		if (bRawValue)
		{
			result.Opacity = g_MaterialRawValues[opacityIndex];
		}
		else
		{
			bool bUnused;
			const uint channel = Material_GetIndex(material.PackedIndices2, OpacityTextureChannelMask, OpacityTextureChannelOffset, bUnused);
			const vec4 val = ReadTexture(opacityIndex, uv);
			result.Opacity = val[channel];
		}
	}
	else
	{
		result.Opacity = 0.5f;
	}

	uint opacityMaskIndex = Material_GetIndex(material.PackedIndices4, OpacityMaskIndexMask, OpacityMaskIndexOffset, bRawValue);
	if (opacityMaskIndex != EG_INVALID_INDEX)
	{
		if (bRawValue)
		{
			result.OpacityMask = g_MaterialRawValues[opacityMaskIndex];
		}
		else
		{
			bool bUnused;
			const uint channel = Material_GetIndex(material.PackedIndices3, OpacityMaskTextureChannelMask, OpacityMaskTextureChannelOffset, bUnused);
			const vec4 val = ReadTexture(opacityMaskIndex, uv);
			result.OpacityMask = val[channel];
		}
	}
	else
	{
		result.OpacityMask = 1.f;
	}

	result.Albedo *= material.TintColor.rgb;
	result.Emissive *= material.EmissiveIntensity;
	result.Opacity = clamp(result.Opacity * material.TintColor.a, 0.f, 1.f);
	result.OpacityMask = clamp(result.OpacityMask * material.TintColor.a, 0.f, 1.f);

	return result;
}
#endif // #ifndef EG_NO_TEXTURES
#endif // #ifndef EG_NO_MATERIALS

#endif
