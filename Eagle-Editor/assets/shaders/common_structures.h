#include "defines.h"

#ifdef __cplusplus

using uint = uint32_t;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif

// Pack 1
const uint MetallnessTextureOffset = 10;
const uint NormalTextureOffset   = 20;

const uint AlbedoTextureMask     = 0x3FF; // 0b11111'11111 - first 10 bits
const uint MetallnessTextureMask = AlbedoTextureMask << MetallnessTextureOffset;
const uint NormalTextureMask     = AlbedoTextureMask << NormalTextureOffset;

// Pack 2
const uint AOTextureOffset = 10;

const uint RoughnessTextureMask = 0x3FF;
const uint AOTextureMask        = RoughnessTextureMask << AOTextureOffset;

struct CPUMaterial
{
	// [0-9]   bits AlbedoTextureIndex
	// [10-19] bits MetallnessTextureIndex
	// [20-29] bits NormalTextureIndex
	uint PackedTextureIndices;

	// [0-9]   bits RoughnessTextureIndex
	// [10-19] bits AOTextureIndex
	// [20-31] bits unused
	uint PackedTextureIndices2;
};

struct ShaderMaterial
{
	uint AlbedoTextureIndex;
	uint MetallnessTextureIndex;
	uint NormalTextureIndex;
	uint RoughnessTextureIndex;
	uint AOTextureIndex;
};

struct PointLight
{
	mat4 ViewProj[6];

	vec3 Position;
	uint unused1_;

	vec3 LightColor;
	float Intensity;
};

struct DirectionalLight
{
	mat4 ViewProj[EG_CASCADES_COUNT];
	float CascadePlaneDistances[EG_CASCADES_COUNT];

	vec3 Direction;
	float Intensity;

	vec3 LightColor;
	uint unused3;

	vec3 Specular;
	uint unused4;
};

struct SpotLight
{
	mat4 ViewProj;

	vec3 Position;
	float InnerCutOffRadians;

	vec3 Direction;
	float OuterCutOffRadians;

	vec3 LightColor;
	float Intensity;
};

#ifndef __cplusplus

void UnpackTextureIndices(CPUMaterial material, out uint albedo, out uint metallness, out uint normal, out uint roughness, out uint ao)
{
	albedo     = (material.PackedTextureIndices & AlbedoTextureMask);
	metallness = (material.PackedTextureIndices & MetallnessTextureMask) >> MetallnessTextureOffset;
    normal     = (material.PackedTextureIndices & NormalTextureMask)     >> NormalTextureOffset;

	roughness = (material.PackedTextureIndices2 & RoughnessTextureMask);
	ao = (material.PackedTextureIndices2 & AOTextureMask) >> AOTextureOffset;
}

#endif
