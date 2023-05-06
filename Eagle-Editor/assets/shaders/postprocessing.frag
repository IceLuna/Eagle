#include "defines.h"
#include "postprocessing_utils.h"
#include "utils.h"

layout(push_constant) uniform PushData
{
    float g_InvGamma;
    float g_Exposure;
    float g_PhotolinearScale;
    float g_WhitePoint;
    uint g_TonemappingMethod;

#ifdef EG_FOG
    float g_FogMin;
    float g_FogMax;
	float g_Density;
	mat4  g_InvProjMat;
	vec3  g_FogColor;
	uint  g_FogEquation;
#endif
};

layout(binding = 0) uniform sampler2D g_Color;
#ifdef EG_FOG
layout(binding = 1) uniform sampler2D g_Depth;
#endif

layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 o_Output;

void main()
{
    vec3 color = texture(g_Color, i_UV).rgb;

#ifdef EG_FOG
    const float depth = texture(g_Depth, i_UV).x;
    const float fogDistance = length(ViewPosFromDepth(g_InvProjMat, i_UV, depth));
    const float fogAlpha = GetFogFactor(g_FogEquation, fogDistance, g_Density, g_FogMin, g_FogMax);
    color = mix(color, g_FogColor, fogAlpha);
#endif

    color = ApplyTonemapping(g_TonemappingMethod, color, g_Exposure, g_WhitePoint, g_PhotolinearScale);
    if (g_TonemappingMethod != TONE_MAPPING_PHOTO_LINEAR)
        color = ApplyGamma(color, g_InvGamma);

    o_Output = vec4(color, 1.f);
}
