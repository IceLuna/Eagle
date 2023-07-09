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

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(binding = 0)
buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(location = 0) flat out int o_EntityID;

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 1.f);

    o_EntityID = a_EntityID;
}
