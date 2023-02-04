#include "mesh_vertex_input_layout.h"

layout(push_constant) uniform PushData
{
    mat4 g_Model;
    mat4 g_ViewProj;
};

void main()
{
    gl_Position = g_ViewProj * g_Model * vec4(a_Position, 1.0);
}
