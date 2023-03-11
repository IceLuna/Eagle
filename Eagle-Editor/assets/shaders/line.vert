layout(location = 0) in vec3 a_Color;
layout(location = 1) in vec3 a_Position;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out vec3 o_Color;

void main()
{
    gl_Position = g_ViewProj * vec4(a_Position, 1.0);
    o_Color = a_Color;
}
