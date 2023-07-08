#include "defines.h"
#include "mesh_vertex_input_layout.h"

layout(binding = 0)
buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

layout(location = 0) flat out int o_ObjectID;

void main()
{
    const mat4 model = g_Transforms[a_PerInstanceData.x];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);
 
    o_ObjectID = int(a_PerInstanceData.z);
}
