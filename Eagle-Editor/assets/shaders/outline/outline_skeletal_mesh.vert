#include "skeletal_mesh_vertex_input_layout.h"
#include "outline/outline_common.h"

layout(scalar, set = EG_OUTLINE_GEOMETRY_SET, binding = 0)
readonly buffer SkinnedVertices
{
    Vertex g_SkinnedVertices[];
};

#ifdef EG_MATERIALS_REQUIRED
layout(scalar, set = EG_OUTLINE_GEOMETRY_SET, binding = 1)
readonly buffer PerInstanceDataBuffer
{
    InstanceData g_InstanceData[];
};
#endif

layout(location = 0) flat out uint o_StyleID;
#ifdef EG_MATERIALS_REQUIRED
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
#endif

void main()
{
    const Vertex vertex = g_SkinnedVertices[g_PushData.VertexOffset + gl_VertexIndex];
    gl_Position = g_PushData.ViewProj * vec4(vertex.Position, 1.0);

    o_StyleID = g_PushData.StyleID;
#ifdef EG_MATERIALS_REQUIRED
    o_TexCoords = vertex.TexCoords;
    o_MaterialIndex = GetMaterialIndex(g_InstanceData[gl_InstanceIndex]);
#endif
}
