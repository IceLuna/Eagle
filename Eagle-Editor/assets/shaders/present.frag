layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D u_Texture;

void main()
{
    outColor = texture(u_Texture, i_UV);
}
