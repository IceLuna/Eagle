#include "defines.h"
#include "skeletal_mesh_vertex_input_layout.h"

layout(scalar, binding = 0)
readonly buffer SkinnedVertices
{
    Vertex g_SkinnedVertices[];
};

layout(scalar, binding = 1)
readonly buffer PerInstanceDataBuffer
{
    InstanceData g_InstanceData[];
};

layout(binding = 2)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

layout(push_constant) uniform PushConstants
{
    uint g_VertexCount;
    uint g_InstanceOffset;
    uint g_VerticesOffset;
};

layout(location = 0) flat out int o_ObjectID;

void main()
{
    // We need an index that's not affected by the offset.
    // So, we need something that goes from [0; InstanceCount)
    const uint instanceIndex = gl_InstanceIndex - g_InstanceOffset;

    const Vertex vertex = g_SkinnedVertices[g_VerticesOffset + g_VertexCount * instanceIndex + gl_VertexIndex];
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];

    gl_Position = g_ViewProjection * vec4(vertex.Position, 1.0);
 
    o_ObjectID = int(instanceData.ObjectID);
}
