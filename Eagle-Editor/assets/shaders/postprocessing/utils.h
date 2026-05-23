#ifndef EG_POSTPROCESSING_UTILS
#define EG_POSTPROCESSING_UTILS

#define TONE_MAPPING_REINHARD 0
#define TONE_MAPPING_FILMIC 1
#define TONE_MAPPING_ACES 2
#define TONE_MAPPING_PHOTO_LINEAR 3
#define TONE_MAPPING_AgX 4
#define TONE_MAPPING_PBR_NEUTRAL 5

#define FOG_LINEAR 0
#define FOG_EXP 1
#define FOG_EXP2 2

vec3 ApplyGamma(vec3 color, float gamma)
{
    return pow(color, vec3(gamma));
}

vec3 ReinhardTonemap(vec3 color)
{
    return color / (vec3(1.f) + color);
}

// Filmic curve from Uncharted 2
// filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 FilmicCurve(vec3 x)
{
    // Curve parameters
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 FilmicTonemap(vec3 color, float white_point)
{
    vec3 numerator = FilmicCurve(color);

    numerator = max(numerator, 0.0);

    vec3 denominator = FilmicCurve(vec3(white_point));
    return numerator / denominator;
}

const vec3 ACESInputVec1 = vec3(0.59719f, 0.35458f, 0.04823f);
const vec3 ACESInputVec2 = vec3(0.07600f, 0.90834f, 0.01566f);
const vec3 ACESInputVec3 = vec3(0.02840f, 0.13383f, 0.83777f);
const vec3 ACESOutputVec1 = vec3(+1.60475f, -0.53108f, -0.07367f);
const vec3 ACESOutputVec2 = vec3(-0.10208f, +1.10813f, -0.00605f);
const vec3 ACESOutputVec3 = vec3(-0.00327f, -0.07276f, +1.07602f);
#define fma(a, b, c) (a * b + c)
vec3 ACESTonemap(vec3 pixel)
{
    //pixel.xyz *= (real)pow(2.0f, exposure);
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    vec3 color = vec3(
        dot(ACESInputVec1, pixel.xyz),
        dot(ACESInputVec2, pixel.xyz),
        dot(ACESInputVec3, pixel.xyz));
    // Apply RRT and ODT
    float a = +0.0245786f;
    float b = -0.000090537f;
    float c = +0.983729f;
    float d = +0.4329510f;
    float e = +0.238081f;
    color = fma(color, (color + a), b) / fma(color, fma(c, color, d), e);
    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    pixel.xyz = vec3(
        dot(ACESOutputVec1, color),
        dot(ACESOutputVec2, color),
        dot(ACESOutputVec3, color));
    // Clamp to [0, 1]
    pixel.xyz = clamp(pixel.xyz, vec3(0), vec3(1));
    return pixel;
}

vec3 PhotoLinearTonemap(vec3 color, float scale)
{
    return color * scale;
}

struct AgXParams
{
    vec3 Slope;
    vec3 Power;
    vec3 Offset;
    float Saturation;
};

// Mean error^2: 3.6705141e-06
vec3 agxDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;

    return +15.5 * x4 * x2
        - 40.14 * x4 * x
        + 31.96 * x4
        - 6.868 * x2 * x
        + 0.4298 * x2
        + 0.1191 * x
        - 0.00232;
}

vec3 agx(vec3 val) {
    const mat3 agx_mat = mat3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104);

    const float min_ev = -12.47393f;
    const float max_ev = 4.026069f;

    // Input transform (inset)
    val = agx_mat * val;

    // Log2 space encoding
    val = clamp(log2(val), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);

    // Apply sigmoid function approximation
    val = agxDefaultContrastApprox(val);

    return val;
}

vec3 agxEotf(vec3 val)
{
    const mat3 agx_mat_inv = mat3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

    // Inverse input transform (outset)
    val = agx_mat_inv * val;

    val = pow(val, vec3(2.2));

    return val;
}

vec3 agxLook(vec3 val, AgXParams agxParams)
{
    // ASC CDL
    val = pow(val * agxParams.Slope + agxParams.Offset, agxParams.Power);

    const vec3 lw = vec3(0.2126, 0.7152, 0.0722);
    float luma = dot(val, lw);

    return luma + agxParams.Saturation * (val - luma);
}

// Based on: https://iolite-engine.com/blog_posts/minimal_agx_implementation
vec3 AgX(vec3 color, AgXParams agxParams)
{
    color = agx(color);
    color = agxLook(color, agxParams); // Optional
    color = agxEotf(color);
    return color;
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
