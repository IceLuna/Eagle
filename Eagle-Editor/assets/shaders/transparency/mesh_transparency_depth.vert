#include "defines.h"
#include "mesh_vertex_input_layout.h"

layout(set = EG_PERSISTENT_SET, binding = 0)
buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

void main()
{
    const mat4 model = g_Transforms[a_PerInstanceData.x];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);
}
