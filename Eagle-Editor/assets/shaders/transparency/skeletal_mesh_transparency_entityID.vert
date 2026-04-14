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

layout(location = 0) flat out int o_ObjectID;

void main()
{
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];
    const uint vertexIndex = instanceData.VertexOffset + gl_VertexIndex;
    const Vertex vertex = g_SkinnedVertices[vertexIndex];

    gl_Position = g_ViewProjection * vec4(vertex.Position, 1.0);
 
    o_ObjectID = int(instanceData.ObjectID);
}
