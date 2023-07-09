layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Color;
layout(location = 2) in vec2  a_TexCoords;
layout(location = 3) in int   a_EntityID;
layout(location = 4) in uint  a_AtlasIndex;
layout(location = 5) in uint  a_TransformIndex;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out int o_EntityID;
layout(location = 3) flat out uint o_AtlasIndex;

layout(binding = 0)
buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 1.0);

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
}
