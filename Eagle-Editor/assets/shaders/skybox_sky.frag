// Resources:
// https://github.com/shff/opengl_sky
// https://github.com/benanders/Hosek-Wilkie
// https://github.com/wwwtyro/glsl-atmosphere/

#include "defines.h"

layout(location = 0) in vec3 i_Pos;
layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const bool s_Cirrus = false;
layout(constant_id = 1) const bool s_Cumulus = false;
layout(constant_id = 2) const uint s_CumulusLayers = 3u;

layout(push_constant) uniform PushData
{
    layout(offset = 64) vec3 g_SunPos;
    float g_SkyIntensity;
    vec3 g_Nitrogen;
    float g_CloudsIntensity;
    float g_ScatterDir;
    float g_Cirrus;
    float g_Cumulus;
};

#define iSteps 8
#define jSteps 4

vec2 RSI(vec3 r0, vec3 rd, float sr)
{
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0)
        return vec2(1e5,-1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

vec3 Atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g)
{
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);

    // Calculate the step size of the primary ray.
    vec2 p = RSI(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0,0,0);
    p.y = min(p.y, RSI(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = 0.0;

    // Initialize accumulators for Rayleigh and Mie scattering.
    vec3 totalRlh = vec3(0,0,0);
    vec3 totalMie = vec3(0,0,0);

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;

    // Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * EG_PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * EG_PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++)
    {
        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

        // Calculate the height of the sample.
        float iHeight = length(iPos) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Calculate the step size of the secondary ray.
        float jStepSize = RSI(iPos, pSun, rAtmos).y / float(jSteps);

        // Initialize the secondary ray time.
        float jTime = 0.0;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        // Sample the secondary ray.
        for (int j = 0; j < jSteps; j++)
        {
            // Calculate the secondary ray sample position.
            vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);

            // Calculate the height of the sample.
            float jHeight = length(jPos) - rPlanet;

            // Accumulate the optical depth.
            jOdRlh += exp(-jHeight / shRlh) * jStepSize;
            jOdMie += exp(-jHeight / shMie) * jStepSize;

            // Increment the secondary ray time.
            jTime += jStepSize;
        }

        // Calculate attenuation.
        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

        // Accumulate scattering.
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;

        // Increment the primary ray time.
        iTime += iStepSize;

    }

    // Calculate and return the final color.
    return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}

float Hash(float n)
{
    return fract(sin(n) * 43758.5453123);
}

float Noise(vec3 x)
{
    vec3 f = fract(x);
    float n = dot(floor(x), vec3(1.0, 157.0, 113.0));
    return mix(mix(mix(Hash(n +   0.0), Hash(n +   1.0), f.x),
                    mix(Hash(n + 157.0), Hash(n + 158.0), f.x), f.y),
                mix(mix(Hash(n + 113.0), Hash(n + 114.0), f.x),
                    mix(Hash(n + 270.0), Hash(n + 271.0), f.x), f.y), f.z);
}

const mat3 m = mat3(0.0, 1.60,  1.20, -1.6, 0.72, -0.96, -1.2, -0.96, 1.28);
float fbm(vec3 p)
{
    float f = 0.0;
    f += Noise(p) / 2; p = m * p * 1.1;
    f += Noise(p) / 4; p = m * p * 1.2;
    f += Noise(p) / 6; p = m * p * 1.3;
    f += Noise(p) / 12; p = m * p * 1.4;
    f += Noise(p) / 24;
    return f;
}

void main()
{
    vec3 color = Atmosphere(
        normalize(i_Pos),               // normalized ray direction
        vec3(0,6372e3,0),               // ray origin
        g_SunPos,                       // position of the sun
        g_SkyIntensity,                 // intensity of the sun
        6371e3,                         // radius of the planet in meters
        6471e3,                         // radius of the atmosphere in meters
        vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
        21e-6,                          // Mie scattering coefficient
        8e3,                            // Rayleigh scale height
        1.2e3,                          // Mie scale height
        g_ScatterDir                    // Mie preferred scattering direction
    );

    outColor = vec4(color, 1.0);

    if (!s_Cirrus && !s_Cumulus)
        return;

    if (i_Pos.y < 0)
        return;

    const float Br = 0.0025; // Rayleigh coefficient
    const float Bm = 0.0003; // Mie coefficient
    const float g =  0.9800; // Mie scattering direction. Should be ALMOST 1.0f

    const vec3 nitrogen = g_Nitrogen;
    const vec3 Kr = Br / pow(nitrogen, vec3(4.0));
    //const vec3 Km = Bm / pow(nitrogen, vec3(0.84));

    const vec3 pos = i_Pos;
    vec3 day_extinction = exp(-exp(-((pos.y + g_SunPos.y * 4.0) * (exp(-pos.y * 16.0) + 0.1) / 80.0) / Br) * (exp(-pos.y * 16.0) + 0.1) * Kr / Br) * exp(-pos.y * exp(-pos.y * 8.0 ) * 4.0) * exp(-pos.y * 2.0) * 4.0;
    vec3 night_extinction = vec3(1.0 - exp(g_SunPos.y)) * 0.2;
    vec3 extinction = mix(day_extinction, night_extinction, -g_SunPos.y * 0.2 + 0.5);

    if (s_Cirrus) // Cirrus Clouds
    {
        float density = smoothstep(1.0 - g_Cirrus, 1.0, fbm(pos.xyz / pos.y * 2.0)) * 0.3;
        outColor.rgb = mix(outColor.rgb, extinction * 4.0 * g_CloudsIntensity, density * pos.y);
    }

    if (s_Cumulus) // Cumulus Clouds
    {
        for (int i = 0; i < s_CumulusLayers; i++)
        {
            float density = smoothstep(1.0 - g_Cumulus, 1.0, fbm((0.7 + float(i) * 0.01) * pos.xyz / pos.y));
            outColor.rgb = mix(outColor.rgb, extinction * density * 5.0 * g_CloudsIntensity, min(density, 1.0) * pos.y);
        }
    }
}
