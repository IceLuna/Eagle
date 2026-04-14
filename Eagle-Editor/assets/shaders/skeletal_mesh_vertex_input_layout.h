#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require

// With this extension, we can define an SSBO of these vertices without worrying about the alignment.
// But it's important to specify `scalar` in its layout! For example:
// 
// layout(scalar, set = 0, binding = 0)
// buffer Vertices
// {
//     Vertex g_Vertices[];
// };
#extension GL_EXT_scalar_block_layout : require

struct SkeletalVertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Tangent;
	vec2 TexCoords;
	uvec2 Weights;
	uvec2 BoneIDs;
};

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Tangent;
	vec2 TexCoords;
};

struct InstanceData
{
	uint TransformIndex; // (highest bit is flag whether it receives decals)
	uint MaterialIndex;
	uint ObjectID;
	uint VertexOffset;
};

#ifdef EG_VERTEX_LAYOUT
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 4) in uvec2 a_Weights; // Each 16bit piece of data is a fp16 representing a weight
layout(location = 5) in uvec2 a_BoneIDs; // Each 16bit piece of data is a u16 representing a boneID

layout(location = 6) in uvec3 a_PerInstanceData; // .x = TransformIndex (highest bit is flag whether it receives decals); .y = MaterialIndex; .z = ObjectID
#endif

float GetWeight(uvec2 weights, uint idx)
{
	vec2 unpackedWeights = unpackHalf2x16(weights[1 - (idx / 2)]);
	return unpackedWeights[1 - (idx % 2)];
}

uint GetBoneID(uvec2 boneIDs, uint idx)
{
	const uint ids = boneIDs[1 - (idx / 2)];

	const uint bits = 16; // sizeof(uint16_t) * 8
	const uint offset = bits * (1 - (idx % 2));
	return (ids >> offset) & 0xFFFF;
}

float GetWeight(SkeletalVertex vertex, uint idx)
{
	return GetWeight(vertex.Weights, idx);
}

uint GetBoneID(SkeletalVertex vertex, uint idx)
{
	return GetBoneID(vertex.BoneIDs, idx);
}

#ifdef EG_VERTEX_LAYOUT
float GetWeight(uint idx)
{
	return GetWeight(a_Weights, idx);
}

uint GetBoneID(uint idx)
{
	return GetBoneID(a_BoneIDs, idx);
}
#endif
