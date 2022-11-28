#ifdef __cplusplus

using uint = uint32_t;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif

const uint SpecularTextureOffset = 10;
const uint NormalTextureOffset   = 20;

const uint DiffuseTextureMask  = 0x3FF; // 0b11111'11111 - first 10 bits
const uint SpecularTextureMask = DiffuseTextureMask << SpecularTextureOffset;
const uint NormalTextureMask   = DiffuseTextureMask << NormalTextureOffset;

struct CPUMaterial
{
	// [0-9]   bits DiffuseTextureIndex
	// [10-19] bits SpecularTextureIndex
	// [20-29] bits NormalTextureIndex
	uint PackedTextureIndices;
};

struct ShaderMaterial
{
	uint DiffuseTextureIndex;
	uint SpecularTextureIndex;
	uint NormalTextureIndex;
};

struct PointLight
{
	mat4 ViewProj[6];
	vec3 Position;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float Intensity;
};

struct DirectionalLight
{
	mat4 ViewProj;
	vec3 Direction;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
};

struct SpotLight
{
	mat4 ViewProj;
	vec3 Position;
	vec3 Direction;

	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	float InnerCutOffAngle;
	float OuterCutOffAngle;
	float Intensity;
};

#ifndef __cplusplus

void UnpackTextureIndices(uint packedTextureIndices, out uint diffuse, out uint specular, out uint normal)
{
    diffuse  =  packedTextureIndices & DiffuseTextureMask;
    specular = (packedTextureIndices & SpecularTextureMask) >> SpecularTextureOffset;
    normal   = (packedTextureIndices & NormalTextureMask)   >> NormalTextureOffset;
}

#endif
