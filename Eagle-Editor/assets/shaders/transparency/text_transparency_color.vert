#include "defines.h"

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

layout(location = 0) out vec4  o_AlbedoRoughness;
layout(location = 1) out vec4  o_EmissiveMetallness;
layout(location = 2) out vec3 o_Normal;
layout(location = 3) out float o_AO;
layout(location = 4) out vec3 o_WorldPos;
layout(location = 5) flat out uint o_AtlasIndex;
layout(location = 6) out vec2 o_TexCoords;
layout(location = 7) out float o_Opacity;

const vec3 s_Normal = vec3(0.0f, 0.0f, 1.0f);

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);
    
    o_AlbedoRoughness = a_AlbedoRoughness;
    o_EmissiveMetallness = a_EmissiveMetallness;

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) < 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    o_AO = a_AO;
    o_WorldPos = vec3(model * vec4(a_Position, 1.0));
    o_AtlasIndex = a_AtlasIndex;
    o_TexCoords = a_TexCoords;
    o_Opacity = a_Opacity;
}