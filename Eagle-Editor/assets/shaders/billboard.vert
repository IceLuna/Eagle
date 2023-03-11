layout(location = 0) in vec3 a_ModelView;
layout(location = 1) in vec2 a_TexCoords;
layout(location = 2) in uint a_TextureIndex;
layout(location = 3) in int  a_EntityID;

layout(push_constant) uniform PushConstants
{
    mat4 g_Proj;
};

layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_TextureIndex;
layout(location = 2) flat out int  o_EntityID;

void main()
{
    gl_Position = g_Proj * vec4(a_ModelView, 1.0);

    o_TexCoords = a_TexCoords;
    o_TextureIndex  = a_TextureIndex;
    o_EntityID = a_EntityID;
}
