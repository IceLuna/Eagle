#extension GL_EXT_nonuniform_qualifier : enable
#include "skeletal_mesh_vertex_input_layout.h"

#ifdef EG_MATERIALS_REQUIRED
const uint s_Set = 1;
#else
const uint s_Set = 0;
#endif

layout(scalar, set = s_Set, binding = 0)
readonly buffer SkinnedVertices
{
    Vertex g_SkinnedVertices[];
};

layout(scalar, set = s_Set, binding = 1)
readonly buffer PerInstanceDataBuffer
{
    InstanceData g_InstanceData[];
};

// For point lights & multi-view depth-pass
#ifdef EG_POINT_LIGHT_PASS
#extension GL_EXT_multiview : enable
layout(set = s_Set, binding = 2) readonly buffer ViewProjectionsBuffer
{
    mat4 g_ViewProjections[];
};

layout(push_constant) uniform PushData
{
    uint g_LightIndex;
};
#endif

#ifndef EG_POINT_LIGHT_PASS
layout(push_constant) uniform PushData
{
    mat4 g_ViewProj;
};
#endif

#ifdef EG_MATERIALS_REQUIRED
layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_MaterialIndex;
#endif

void main()
{
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];
    
    const uint vertexIndex = GetVertexOffset(instanceData) + gl_VertexIndex;
    const Vertex vertex = g_SkinnedVertices[vertexIndex];

    const vec4 worldPos = vec4(vertex.Position, 1.0);
#ifdef EG_POINT_LIGHT_PASS
    gl_Position = g_ViewProjections[g_LightIndex * 6 + gl_ViewIndex] * worldPos;
#elif defined(EG_SPOT_LIGHT_PASS)
    gl_Position = g_ViewProj * worldPos;
#else
    gl_Position = g_ViewProj * worldPos;
#endif

#ifdef EG_MATERIALS_REQUIRED
    o_TexCoords = vertex.TexCoords;
    o_MaterialIndex = GetMaterialIndex(instanceData);
#endif
}
