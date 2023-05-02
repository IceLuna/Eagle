#include "utils.h"

layout(location = 0) in vec2 i_UV;
layout(location = 0) out float outColor;

layout(push_constant) uniform PushConstants
{
    vec2 g_TexelSize;
};

layout(binding = 0) uniform sampler2D g_SSAO;

void main()
{
    float result = 0.f;

    for (int x = -2; x < 2; ++x)
       for (int y = -2; y < 2; ++y)
       {
           vec2 offset = vec2(x, y) * g_TexelSize;
           result += texture(g_SSAO, i_UV + offset).x;
       }
    
    const float invNumSamples = 1.f / 16.f;
    outColor = result * invNumSamples;
}
