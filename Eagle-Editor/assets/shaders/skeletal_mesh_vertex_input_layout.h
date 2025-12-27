#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 4) in uvec2 a_Weights; // Each 16bit piece of data is a fp16 representing a weight
layout(location = 5) in uvec2 a_BoneIDs; // Each 16bit piece of data is a u16 representing a boneID

layout(location = 6) in uvec3 a_PerInstanceData; // .x = TransformIndex (highest bit is flag whether it receives decals); .y = MaterialIndex; .z = ObjectID

float GetWeight(uint idx)
{
	vec2 weights = unpackHalf2x16(a_Weights[1 - (idx / 2)]);
	return weights[1 - (idx % 2)];
}

uint GetBoneID(uint idx)
{
	const uint ids = a_BoneIDs[1 - (idx / 2)];

	const uint bits = 16; // sizeof(uint16_t) * 8
	const uint offset = bits * (1 - (idx % 2));
	return (ids >> offset) & 0xFFFF;
}
