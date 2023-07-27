#include "text_unlit_vertex_input_layout.h"

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

#ifdef EG_JITTER
layout(set = 2, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 1.0);
#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
}
