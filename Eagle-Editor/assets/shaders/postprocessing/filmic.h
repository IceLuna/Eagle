#ifndef EG_POSTPROCESSING_FILMIC
#define EG_POSTPROCESSING_FILMIC

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

#endif // EG_POSTPROCESSING_FILMIC
