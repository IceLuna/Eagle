#include "defines.h"
#include "mesh_vertex_input_layout.h"

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
    const uint transformIndex = a_PerInstanceData.x & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    const vec4 worldPos = model * vec4(a_Position, 1.0);
    gl_Position = g_ViewProjection * worldPos;
}
