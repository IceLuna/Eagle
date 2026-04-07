#include "defines.h"
#include "sprite_vertex_input_layout.h"

layout(binding = 0) readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(binding = 1)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProjection * model * vec4(s_QuadVertexPosition[vertexID], 1.f);
}
