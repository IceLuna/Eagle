// With this extension, we can define an SSBO of these vertices without worrying about the alignment.
// But it's important to specify `scalar` in its layout! For example:
// 
// layout(scalar, set = 0, binding = 0)
// buffer Vertices
// {
//     Vertex g_Vertices[];
// };
#extension GL_EXT_scalar_block_layout : require

#include "defines.h"

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
	uint PackedTransformIndex;
	uint PackedMaterialIndex;
	uint ObjectID;
	uint VertexOffset;
};

#ifdef EG_VERTEX_LAYOUT
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;
layout(location = 4) in uvec2 a_Weights; // Each 16bit piece of data is a unorm16 representing a weight
layout(location = 5) in uvec2 a_BoneIDs; // Each 16bit piece of data is a u16 representing a boneID

layout(location = 6) in uvec4 a_PerInstanceData;
#endif

float GetWeight(uvec2 weights, uint idx)
{
	uint unorm = weights[1 - (idx / 2)];
	uint bitsIndex = (1 - (idx % 2)); // If 0, lower 16 bits, else - higher 16 bits
	uint offset = (~(bitsIndex - 1u)) & 16; // If bitsIndex is 0, offset will be 0, otherwise - 16
	return ((unorm >> offset) & 0xFFFF) * (1.0 / 65535.0);
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

uint GetTransformIndex(uint data)
{
	return data & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
}

uint GetMaterialIndex(uint data)
{
	return data & (~EG_CASTS_SHADOWS_MASK); // Get all but the highest bit
}

uint GetObjectID(uint data)
{
	return data;
}

bool DoesReceiveDecals(uint data)
{
	return (data & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK;
}

bool DoesCastShadows(uint data)
{
	return (data & EG_CASTS_SHADOWS_MASK) == EG_CASTS_SHADOWS_MASK;
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

uint GetTransformIndex()
{
	return GetTransformIndex(a_PerInstanceData.x);
}

uint GetMaterialIndex()
{
	return GetMaterialIndex(a_PerInstanceData.y);
}

uint GetObjectID()
{
	return GetObjectID(a_PerInstanceData.z);
}

uint GetVertexOffset(InstanceData instance)
{
	return a_PerInstanceData.w;
}

bool DoesReceiveDecals()
{
	return DoesReceiveDecals(a_PerInstanceData.x);
}

bool DoesCastShadows()
{
	return DoesCastShadows(a_PerInstanceData.y);
}
#else
uint GetTransformIndex(InstanceData instance)
{
	return GetTransformIndex(instance.PackedTransformIndex);
}

uint GetMaterialIndex(InstanceData instance)
{
	return GetMaterialIndex(instance.PackedMaterialIndex);
}

uint GetObjectID(InstanceData instance)
{
	return GetObjectID(instance.ObjectID);
}

uint GetVertexOffset(InstanceData instance)
{
	return instance.VertexOffset;
}

bool DoesReceiveDecals(InstanceData instance)
{
	return DoesReceiveDecals(instance.PackedTransformIndex);
}

bool DoesCastShadows(InstanceData instance)
{
	return DoesCastShadows(instance.PackedMaterialIndex);
}
#endif
