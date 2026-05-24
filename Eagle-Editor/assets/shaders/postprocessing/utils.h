#ifndef EG_POSTPROCESSING_UTILS
#define EG_POSTPROCESSING_UTILS

#define TONE_MAPPING_REINHARD 0
#define TONE_MAPPING_FILMIC 1
#define TONE_MAPPING_ACES 2
#define TONE_MAPPING_PHOTO_LINEAR 3
#define TONE_MAPPING_AgX 4
#define TONE_MAPPING_PBR_NEUTRAL 5
#define TONE_MAPPING_GT7 6

#define FOG_LINEAR 0
#define FOG_EXP 1
#define FOG_EXP2 2

#include "postprocessing/aces.h"
#include "postprocessing/filmic.h"
#include "postprocessing/agx.h"
#include "postprocessing/gt7.h"

vec3 ApplyGamma(vec3 color, float gamma)
{
    return pow(color, vec3(gamma));
}

vec3 ReinhardTonemap(vec3 color)
{
    return color / (vec3(1.f) + color);
}

vec3 PhotoLinearTonemap(vec3 color, float scale)
{
    return color * scale;
}

// Khronos PBR neutral tonemapping
vec3 PBRNeutral(vec3 color)
{
    const float F90 = 0.04f;
    const float ks = 0.8f - F90;
    const float kd = 0.15f;

    float x = min(color.x, min(color.y, color.z));
    float offset = x < (2.0f * F90) ? x - (1.0f / (4.0f * F90)) * x * x : 0.04f;
    color -= offset;

    float p = max(color.x, max(color.y, color.z));
    if (p <= ks)
    {
        return color;
    }

    float d = 1.0f - ks;
    float pn = 1.0f - d * d / (p + d - ks);

    float g = 1.0f / (kd * (p - pn) + 1.0f);
    return mix(vec3(pn), color * (pn / p), g);
}

vec3 ApplyTonemapping(const uint tonemapping_method, vec3 color, float exposure, float white_point, float photolinear_scale, AgXParams agxParams)
{
    color *= exposure;

    switch (tonemapping_method)
    {
        case TONE_MAPPING_REINHARD:     return ReinhardTonemap(color);
        case TONE_MAPPING_FILMIC:       return FilmicTonemap(color, white_point);
        case TONE_MAPPING_ACES:         return ACESTonemap(color);
        case TONE_MAPPING_PHOTO_LINEAR: return PhotoLinearTonemap(color, photolinear_scale);
        case TONE_MAPPING_AgX:          return AgX(color, agxParams);
        case TONE_MAPPING_PBR_NEUTRAL:  return PBRNeutral(color);
        case TONE_MAPPING_GT7:          return GT7(color);
        default: return color;
    }
}

float FogLinear(float distance, float fogMin, float fogMax)
{
    const float result = (fogMax - distance) / (fogMax - fogMin);
    return 1.f - clamp(result, 0.f, 1.f);
}

float FogExp(float distance, float density)
{
    const float result = exp(-distance * density);
    return 1.f - clamp(result, 0.f, 1.f);
}

float FogExp2(float distance, float density)
{
    const float dd = distance * density;
    const float result = exp(-dd * dd);
    return 1.f - clamp(result, 0.f, 1.f);
}

float GetFogFactor(uint fogEquation, float distance, float density, float fogMin, float fogMax)
{
    switch (fogEquation)
    {
        case FOG_LINEAR: return FogLinear(distance, fogMin, fogMax);
        case FOG_EXP: return FogExp(distance, density);
        case FOG_EXP2: return FogExp2(distance, density);
        default: return FogLinear(distance, fogMin, fogMax);
    }
}

float Luminance(vec3 rgb)
{
    return dot(rgb, vec3(0.2126729, 0.7151522, 0.0721750));
}

// [Karis2013] proposed reducing the dynamic range before averaging
vec4 KarisAvg(vec4 c)
{
    return c / (1.0 + Luminance(c.rgb));
}

#endif
