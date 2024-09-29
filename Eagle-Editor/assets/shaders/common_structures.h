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

const uint IsRawValueMask         = 0x8000;
const uint MaterialIndexMask      = 0xFFFF; // 16 bits per index

// Pack 1
const uint AlbedoIndexOffset      = 0;
const uint MetalnessIndexOffset   = 16;
const uint AlbedoIndexMask        = MaterialIndexMask << AlbedoIndexOffset;
const uint MetalnessIndexMask     = MaterialIndexMask << MetalnessIndexOffset;

// Pack 2
const uint NormalIndexOffset      = 0;
const uint RoughnessIndexOffset   = 16;
const uint NormalIndexMask        = MaterialIndexMask << NormalIndexOffset;
const uint RoughnessIndexMask     = MaterialIndexMask << RoughnessIndexOffset;

// Pack 3
const uint AOIndexOffset          = 0;
const uint EmissiveIndexOffset    = 16;
const uint AOIndexMask            = MaterialIndexMask << AOIndexOffset;
const uint EmissiveIndexMask      = MaterialIndexMask << EmissiveIndexOffset;

// Pack 4
const uint OpacityIndexOffset     = 0;
const uint OpacityMaskIndexOffset = 16;
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
	// [0-15]  bits Albedo Index
	// [16-31] bits Metalness Index
	uint PackedIndices;

	// [0-15]  bits Normal Index
	// [16-31] bits Roughness Index
	uint PackedIndices2;

	// [0-15]  bits AO Index
	// [16-31] bits Emissive Index
	uint PackedIndices3;
	
	// [0-15]  bits Opacity Index
	// [16-31] bits Opacity Mask Index
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
	mat4 ViewProj[6];

	vec3 Position;
	float Radius2; // Sign bit is used as a flag for `bCastsShadows`

	vec3 LightColor;
	float VolumetricFogIntensity; // Sign bit is used as a flag for `bVolumetricLight`
};

struct DirectionalLight
{
	mat4 ViewProj[EG_CASCADES_COUNT];
	float CascadePlaneDistances[EG_CASCADES_COUNT];

	vec3 Direction;
	float VolumetricFogIntensity;

	vec3 LightColor;
	uint bCastsShadows;

	vec3 Specular;
	uint bVolumetricLight;

	vec3 Ambient;
	uint unused;
};

struct SpotLight
{
	mat4 ViewProj;

	vec3 Position;
	float InnerCutOffRadians;

	vec3 Direction;
	float OuterCutOffRadians;

	vec3 LightColor;
	float VolumetricFogIntensity;

	float unused1;
	float Distance2;
	uint bCastsShadows;
	uint bVolumetricLight;
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
