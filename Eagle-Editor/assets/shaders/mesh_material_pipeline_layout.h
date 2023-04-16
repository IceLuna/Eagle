#ifndef EG_MESH_MATERIAL_PIPELINE_LAYOUT
#define EG_MESH_MATERIAL_PIPELINE_LAYOUT

#include "defines.h"
#include "common_structures.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MATERIALS)
buffer Materials
{
	CPUMaterial g_Materials[];
};

ShaderMaterial FetchMaterial(uint index)
{
	ShaderMaterial result;
	CPUMaterial material = g_Materials[index];

	result.TintColor = material.TintColor;
	result.EmissiveIntensity = material.EmissiveIntensity;
	result.TilingFactor = material.TilingFactor;
	UnpackTextureIndices(material, result.AlbedoTextureIndex, result.MetallnessTextureIndex, result.NormalTextureIndex, result.RoughnessTextureIndex, result.AOTextureIndex, result.EmissiveTextureIndex);

	return result;
}

#endif
