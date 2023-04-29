#include "defines.h"
#include "postprocessing_utils.h"

layout(push_constant) uniform PushData
{
    layout(offset = 8) float g_InvGamma;
    layout(offset = 12) float g_Exposure;
    layout(offset = 16) float g_PhotolinearScale;
    layout(offset = 20) float g_WhitePoint;
    layout(offset = 24) uint g_TonemappingMethod;
};

layout(set = 0, binding = 0) uniform sampler2D g_Color;

layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 o_Output;

void main()
{
    vec3 color = texture(g_Color, i_UV).rgb;
    color = ApplyTonemapping(g_TonemappingMethod, color, g_Exposure, g_WhitePoint, g_PhotolinearScale);
    if (g_TonemappingMethod != TONE_MAPPING_PHOTO_LINEAR)
        color = ApplyGamma(color, g_InvGamma);

    o_Output = vec4(color, 1.f);
}
