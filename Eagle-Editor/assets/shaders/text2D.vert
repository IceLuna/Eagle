layout(location = 0) in vec3  a_Color;
layout(location = 1) in uint  a_AtlasIndex;
layout(location = 2) in vec2  a_Position;
layout(location = 3) in vec2  a_TexCoords;
layout(location = 4) in vec2  a_Scale;
layout(location = 5) in int   a_EntityID;
layout(location = 6) in float a_Opacity;

layout(push_constant) uniform PushConstants
{
    mat4 g_Proj;
    vec2 g_Size;
    float g_InvAspectRatio;
};

layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out int o_EntityID;
layout(location = 3) flat out uint o_AtlasIndex;
layout(location = 4) flat out float o_Opacity;

void main()
{
    const vec2 projSize = vec2(1920.f, 1080.f);
    const vec2 sizeDiff = g_Size / projSize;

    vec2 pos = (a_Scale * vec2(g_InvAspectRatio, 1.f) + a_Position / sizeDiff);
    pos *= g_Size;
    gl_Position = g_Proj * vec4(pos, 0.f, 1.0);

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_AtlasIndex = a_AtlasIndex;
    o_EntityID = a_EntityID;
    o_Opacity = a_Opacity;
}
