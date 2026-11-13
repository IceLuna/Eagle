layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in float a_Opacity;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out vec4 o_Color;

void main()
{
    gl_Position = g_ViewProj * vec4(a_Position, 1.0);
    o_Color = vec4(a_Color, clamp(a_Opacity, 0.0, 1.0));
}
