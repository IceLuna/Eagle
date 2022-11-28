#include "pbr_pipeline_layout.h"

layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    vec3 ViewPosition;
};

void main()
{
    //outColor = texture(g_NormalTexture, i_UV);
    outColor = texture(g_AlbedoTexture, i_UV);
}
