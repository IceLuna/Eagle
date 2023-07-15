#include "text_lit_vertex_input_layout.h"

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
