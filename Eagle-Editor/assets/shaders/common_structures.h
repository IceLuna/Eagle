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

const uint IsRawValueMask          = 0x2000;
const uint MaterialIndexMask       = 0x3FFF; // 14 bits per index
const uint TextureChannelIndexMask = 0x3; // 0b11 (two bits)

// Pack 1
const uint AlbedoIndexOffset               = 0;
const uint MetalnessIndexOffset            = 14;
const uint MetalnessTextureChannelOffset   = 28;
const uint RoughnessTextureChannelOffset   = 30;

const uint AlbedoIndexMask                 = MaterialIndexMask       << AlbedoIndexOffset;
const uint MetalnessIndexMask              = MaterialIndexMask       << MetalnessIndexOffset;
const uint MetalnessTextureChannelMask     = TextureChannelIndexMask << MetalnessTextureChannelOffset;
const uint RoughnessTextureChannelMask     = TextureChannelIndexMask << RoughnessTextureChannelOffset;

// Pack 2
const uint NormalIndexOffset               = 0;
const uint RoughnessIndexOffset            = 14;
const uint AOTextureChannelOffset          = 28;
const uint OpacityTextureChannelOffset     = 30;

const uint NormalIndexMask                 = MaterialIndexMask       << NormalIndexOffset;
const uint RoughnessIndexMask              = MaterialIndexMask       << RoughnessIndexOffset;
const uint AOTextureChannelMask            = TextureChannelIndexMask << AOTextureChannelOffset;
const uint OpacityTextureChannelMask       = TextureChannelIndexMask << OpacityTextureChannelOffset;

// Pack 3
const uint AOIndexOffset                   = 0;
const uint EmissiveIndexOffset             = 14;
const uint OpacityMaskTextureChannelOffset = 28;

const uint AOIndexMask                     = MaterialIndexMask       << AOIndexOffset;
const uint EmissiveIndexMask               = MaterialIndexMask       << EmissiveIndexOffset;
const uint OpacityMaskTextureChannelMask   = TextureChannelIndexMask << OpacityMaskTextureChannelOffset;

// Pack 4
const uint OpacityIndexOffset     = 0;
const uint OpacityMaskIndexOffset = 14;
const uint OpacityIndexMask       = MaterialIndexMask << OpacityIndexOffset;
const uint OpacityMaskIndexMask   = MaterialIndexMask << OpacityMaskIndexOffset;

struct CPUMaterial
{
	vec4 TintColor;

	vec3 EmissiveIntensity;
	float TilingFactor;

	// Packed indices. 16bits for each index.
	// Highest bit of the index is used to indicate that the index points into the buffer of raw values (not textures)
	//
	// [0-13]  bits Albedo Index
	// [14-27] bits Metalness Index
	// [28-29] bits Metalness Texture Channel
	// [30-31] bits Roughness Texture Channel
	uint PackedIndices;

	// [0-13]  bits Normal Index
	// [14-27] bits Roughness Index
	// [28-29] bits AO Texture Channel
	// [30-31] bits Opacity Texture Channel
	uint PackedIndices2;

	// [0-13]  bits AO Index
	// [14-27] bits Emissive Index
	// [28-29] bits Opacity Mask Texture Channel
	// [30-31] bits Unused
	uint PackedIndices3;
	
	// [0-13]  bits Opacity Index
	// [14-27] bits Opacity Mask Index
	// [28-31] bits Unused
	uint PackedIndices4;

#ifdef __cplusplus
	CPUMaterial()
		: TintColor(1.f), EmissiveIntensity(0.f), TilingFactor(1.f)
		, PackedIndices(0), PackedIndices2(0), PackedIndices3(0), PackedIndices4(0)
	{
	}
	
	static CPUMaterial Convert(const std::shared_ptr<Eagle::Material>& material, std::vector<float>& rawValues);
#endif
};

struct ShaderMaterial
{
	vec3 Albedo;
	float Metalness;

	vec3 Emissive;
	float Roughness;
	
	float AO;
	float Opacity;
	float OpacityMask;
	float TilingFactor;

	// Normal can only be a texture, so we keep it as an index so that the code can verify if the texture was set. If not, geometry normals will be used
	uint NormalTextureIndex;
};

struct PointLight
{
	vec3 Position;
	float Radius2; // Sign bit is used as a flag for `bCastsShadows`

	vec3 LightColor;
	float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`

	uint ShadowMapIndex;
	uint ViewProjOffset; // Note: it's invalid to use it on shader side because point light transforms aren't uploaded. Currently, used to fetch it on the CPU side
	float Radius;
	uint Padding0;
};

struct DirectionalLight
{
	float CascadePlaneDistances[EG_CASCADES_COUNT];

	vec3 Direction;
	uint ViewProjOffset; // Offset into the transforms buffer

	vec3 LightColor;
	uint Flags;

	vec3 Ambient;
	float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`
};

struct SpotLight
{
	vec3 Position;
	float InnerCutOffRadians;

	vec3 Direction;
	float OuterCutOffRadians;

	vec3 LightColor;
	uint ViewProjOffset; // Offset into the transforms buffer

	float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`
	float Distance;
	uint bCastsShadows;
	uint ShadowMapIndex;
};

#ifndef __cplusplus

uint Material_GetIndex(uint packed, uint mask, uint offset, out bool bRawValue)
{
	uint unpacked = (packed & mask) >> offset;
	bRawValue = (unpacked & IsRawValueMask) != 0;

	return unpacked & (~IsRawValueMask);
}

ShaderMaterial ShaderMaterial_Default()
{
	ShaderMaterial result;
	result.TilingFactor = 1.f;
	result.Albedo = vec3(1, 0, 1);
	result.Metalness = 0.f;
	result.Emissive = vec3(0.f);
	result.Roughness = EG_MIN_ROUGHNESS;
	result.AO = 1.f;
	result.Opacity = 1.f;
	result.OpacityMask = 1.f;
	result.NormalTextureIndex = EG_INVALID_INDEX;
	return result;
}

#endif

#endif
