layout(location = 0) in vec4  a_AlbedoRoughness;
layout(location = 1) in vec4  a_EmissiveMetallness;
layout(location = 2) in vec3  a_WorldPosition;
layout(location = 3) in int   a_EntityID;
layout(location = 4) in vec3  a_Normal;
layout(location = 5) in uint  a_AtlasIndex;
layout(location = 6) in vec2  a_TexCoords;
layout(location = 7) in float a_AO;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out vec4 o_AlbedoRoughness;
layout(location = 1) out vec4 o_EmissiveMetallness;
layout(location = 2) out vec3 o_Normal;
layout(location = 3) flat out int o_EntityID;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_AtlasIndex;
layout(location = 6) out float o_AO;

void main()
{
    gl_Position = g_ViewProj * vec4(a_WorldPosition, 1.0);

    o_AlbedoRoughness = a_AlbedoRoughness;
    o_EmissiveMetallness = a_EmissiveMetallness;
    o_Normal = a_Normal;
    o_TexCoords = a_TexCoords;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
    o_AO = a_AO;
}
