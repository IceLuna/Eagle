layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 i_Color;

layout(set = 0, binding = 0) uniform sampler2D u_Unused;
layout(set = 1, binding = 0) uniform sampler2D u_Albedo;

void main()
{
    outColor = i_Color;
}
