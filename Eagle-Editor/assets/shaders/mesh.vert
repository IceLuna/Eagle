layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_TexCoords;

layout(push_constant) uniform PushConstants
{
    mat4 g_Model;
    mat4 g_ViewProj;
};

void main()
{
    gl_Position = g_ViewProj * g_Model * vec4(a_Position, 1.0);
}
