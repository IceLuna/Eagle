layout(location = 0) in vec3  a_Tint;
layout(location = 1) in uint  a_TextureIndex;
layout(location = 2) in vec2  a_Position;
layout(location = 3) in vec2  a_Scale;
layout(location = 4) in int   a_EntityID;
layout(location = 5) in float a_Opacity;

layout(push_constant) uniform PushConstants
{
    mat4 g_Proj;
    vec2 g_Size;
    float g_InvAspectRatio;
};

layout(location = 0) out vec4 o_Tint_Opacity;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out int o_EntityID;
layout(location = 3) flat out uint o_TextureIndex;

const vec2 s_TexCoords[4] = {
	vec2(0.f, 1.f),
	vec2(1.f, 1.f),
	vec2(1.f, 0.f),
	vec2(0.f, 0.f)
};

void main()
{
    const vec2 projSize = vec2(1920.f, 1080.f);
    const vec2 sizeDiff = g_Size / projSize;

    vec2 pos = (a_Scale * vec2(g_InvAspectRatio, 1.f) + a_Position / sizeDiff);
    pos *= g_Size;
    gl_Position = g_Proj * vec4(pos, 0.f, 1.0);

    const uint vertexID = gl_VertexIndex % 4u;
    o_TexCoords = s_TexCoords[vertexID];
    o_Tint_Opacity = vec4(a_Tint, a_Opacity);
    o_TextureIndex = a_TextureIndex;
    o_EntityID = a_EntityID;
}
