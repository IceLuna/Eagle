#ifndef EG_POSTPROCESSING_UTILS
#define EG_POSTPROCESSING_UTILS

#define TONE_MAPPING_NONE 0
#define TONE_MAPPING_REINHARD 1
#define TONE_MAPPING_FILMIC 2
#define TONE_MAPPING_ACES 3
#define TONE_MAPPING_PHOTO_LINEAR 4

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

vec3 ApplyTonemapping(const uint tonemapping_method, vec3 color, float exposure, float white_point, float photolinear_scale)
{
    color *= exposure;

    switch (tonemapping_method)
    {
        case TONE_MAPPING_REINHARD:     return ReinhardTonemap(color);
        case TONE_MAPPING_FILMIC:       return FilmicTonemap(color, white_point);
        case TONE_MAPPING_ACES:         return ACESTonemap(color);
        case TONE_MAPPING_PHOTO_LINEAR: return PhotoLinearTonemap(color, photolinear_scale);
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

#endif
