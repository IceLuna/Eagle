layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 i_Color;

void main()
{
    outColor = vec4(i_Color, 1.f);
}
