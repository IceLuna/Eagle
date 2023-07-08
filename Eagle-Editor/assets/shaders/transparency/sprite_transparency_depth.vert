#include "defines.h"
#include "sprite_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(set = EG_PERSISTENT_SET, binding = 0)
buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

void main()
{
    const uint transformIndex = a_BufferIndex;

    const mat4 model = g_Transforms[transformIndex];

    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);
}
