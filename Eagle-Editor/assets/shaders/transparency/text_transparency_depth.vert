layout(location = 0) in vec4  a_AlbedoRoughness;
layout(location = 1) in vec4  a_EmissiveMetallness;
layout(location = 2) in vec3  a_Position;
layout(location = 3) in int   a_EntityID;
layout(location = 4) in vec2  a_TexCoords;
layout(location = 5) in uint  a_AtlasIndex;
layout(location = 6) in float a_AO;
layout(location = 7) in float a_Opacity;
layout(location = 8) in uint a_TransformIndex;

layout(binding = 0)
buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

// Outputs
layout(location = 0) out vec2      o_TexCoords;
layout(location = 1) flat out uint o_AtlasIndex;

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);

    o_TexCoords = a_TexCoords;
    o_AtlasIndex = a_AtlasIndex;
}
