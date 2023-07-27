#include "defines.h"
#include "text_lit_vertex_input_layout.h"

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
layout(location = 1) flat out uint o_AtlasIndex;
layout(location = 2) out vec2 o_TexCoords;

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 1.f);

    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
    o_TexCoords = a_TexCoords;
}
