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

struct InstanceData
{
	uint PackedTransformIndex;
	uint PackedMaterialIndex;
	uint ObjectID;
};

#ifndef EG_COMPUTE_VERTEX_LAYOUT
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoords;

layout(location = 4) in uvec3 a_PerInstanceData;
#endif

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

#ifdef EG_COMPUTE_VERTEX_LAYOUT
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

bool DoesReceiveDecals(InstanceData instance)
{
	return DoesReceiveDecals(instance.PackedTransformIndex);
}

bool DoesCastShadows(InstanceData instance)
{
	return DoesCastShadows(instance.PackedMaterialIndex);
}
#else
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

bool DoesReceiveDecals()
{
	return DoesReceiveDecals(a_PerInstanceData.x);
}

bool DoesCastShadows()
{
	return DoesCastShadows(a_PerInstanceData.y);
}
#endif

