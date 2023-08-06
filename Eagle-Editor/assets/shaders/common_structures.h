#ifndef EG_COMMON_STRUCTURES
#define EG_COMMON_STRUCTURES

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
const uint EmissiveTextureOffset = 20;

const uint RoughnessTextureMask = 0x3FF;
const uint AOTextureMask        = RoughnessTextureMask << AOTextureOffset;
const uint EmissiveTextureMask  = RoughnessTextureMask << EmissiveTextureOffset;

// Pack 3
const uint OpacityTextureOffset = 10;

const uint OpacityTextureMask     = 0x3FF; // 0b11111'11111 - first 10 bits
const uint OpacityMaskTextureMask = OpacityTextureMask << OpacityTextureOffset;

struct CPUMaterial
{
	vec4 TintColor;

	vec3 EmissiveIntensity;
	float TilingFactor;

	// [0-9]   bits AlbedoTextureIndex
	// [10-19] bits MetallnessTextureIndex
	// [20-29] bits NormalTextureIndex
	uint PackedTextureIndices;

	// [0-9]   bits RoughnessTextureIndex
	// [10-19] bits AOTextureIndex
	// [20-29] bits EmissiveTextureIndex
	uint PackedTextureIndices2;

	// [0-9]   bits OpacityTextureIndex
	uint PackedTextureIndices3;
	uint padding1;

#ifdef __cplusplus
	CPUMaterial()
		: TintColor(1.f), EmissiveIntensity(0.f), TilingFactor(1.f)
		, PackedTextureIndices(0), PackedTextureIndices2(0), PackedTextureIndices3(0)
	{
	}
	
	CPUMaterial(const std::shared_ptr<Eagle::Material>& material);
	CPUMaterial& operator=(const std::shared_ptr<Eagle::Texture2D>& texture);
	CPUMaterial& operator=(const std::shared_ptr<Eagle::Material>& material);
#endif
};

struct ShaderMaterial
{
	vec4 TintColor;
	vec3 EmissiveIntensity;
	float TilingFactor;

	uint AlbedoTextureIndex;
	uint MetallnessTextureIndex;
	uint NormalTextureIndex;
	uint RoughnessTextureIndex;
	uint AOTextureIndex;
	uint EmissiveTextureIndex;
	uint OpacityTextureIndex;
	uint OpacityMaskTextureIndex;
};

struct PointLight
{
	mat4 ViewProj[6];

	vec3 Position;
	float Radius;

	vec3 LightColor;
	float Intensity; // Sign bit is used as a flag for `bCastsShadows`
};

struct DirectionalLight
{
	mat4 ViewProj[EG_CASCADES_COUNT];
	float CascadePlaneDistances[EG_CASCADES_COUNT];

	vec3 Direction;
	float Intensity;

	vec3 LightColor;
	uint bCastsShadows;

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

	vec2 unused1;
	float Distance;
	uint bCastsShadows;
};

#ifndef __cplusplus

void UnpackTextureIndices(CPUMaterial material, out uint albedo, out uint metallness, out uint normal, out uint roughness, out uint ao, out uint emissive, out uint opacity, out uint opacityMask)
{
	albedo     = (material.PackedTextureIndices & AlbedoTextureMask);
	metallness = (material.PackedTextureIndices & MetallnessTextureMask) >> MetallnessTextureOffset;
    normal     = (material.PackedTextureIndices & NormalTextureMask)     >> NormalTextureOffset;

	roughness = (material.PackedTextureIndices2 & RoughnessTextureMask);
	ao        = (material.PackedTextureIndices2 & AOTextureMask)       >> AOTextureOffset;
	emissive  = (material.PackedTextureIndices2 & EmissiveTextureMask) >> EmissiveTextureOffset;

	opacity     = (material.PackedTextureIndices3 & OpacityTextureMask);
	opacityMask = (material.PackedTextureIndices3 & OpacityMaskTextureMask) >> OpacityTextureOffset;
}

#endif

#endif
