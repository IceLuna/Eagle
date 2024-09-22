#include "defines.h"
#include "text/text_lit_vertex_input_layout.h"

layout(binding = 0)
readonly buffer TransformsBuffer
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
    const uint transformIndex = a_TransformIndex & (EG_RECEIVES_DECALS_MASK - 1); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 0.f, 1.0);

    o_TexCoords = a_TexCoords;
    o_AtlasIndex = a_AtlasIndex;
}
