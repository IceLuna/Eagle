#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "common_structures.h"
#include "utils.h"

layout(set = EG_PERSISTENT_SET, binding = 0) uniform sampler2D g_Textures[MAX_TEXTURES];

layout(set = EG_PERSISTENT_SET, binding = 1)
buffer Materials
{
	CPUMaterial g_Materials[];
};

vec4 ReadTexture(uint index, vec2 uv)
{
	return texture(g_Textures[nonuniformEXT(index)], uv);
}

ShaderMaterial FetchMaterial(uint index)
{
	ShaderMaterial result;
	CPUMaterial material = g_Materials[index];

	UnpackTextureIndices(material, result.AlbedoTextureIndex, result.MetallnessTextureIndex, result.NormalTextureIndex, result.RoughnessTextureIndex, result.AOTextureIndex);

	return result;
}
