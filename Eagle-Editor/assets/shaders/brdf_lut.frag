#include "defines.h"
#include "pbr_utils.h"

layout(location = 0) in vec2 i_UV;

layout(location = 0) out vec2 outColor;

// Lambda is a part of G1 shadowing term from the same paper.
float GGX_Lambda_Aniso(vec2 a, vec3 v)
{
    float xv = v.x;
    float yv = v.z;
    float zv = clamp(v.y, 0.0, 1.0);

    // q can be Inf here.
    float q = (a.x * xv * a.x * xv + a.y * yv * a.y * yv) / (zv * zv);

    // We are allowed to return Inf from here, in this case G1 and G2 shadowing -> 0.
    return 0.5 * (-1.0 + sqrt(1.0 + q));
}

// G1 defines a fraction of microsurface normals visible from the direction v.
// This particular implementation does not depend on the microsurface normal itself.
float GGX_G1_Aniso(vec2 a, vec3 v)
{
    return 1.0 / (1.0 + GGX_Lambda_Aniso(a, v));
}

// G2 defines a fraction of microsurface normals visible from both incoming 
// and outgoing directions. This particular implementation does not depend on the microsurface normal itself.
float GGX_G2_Aniso(vec2 a, vec3 incoming, vec3 outgoing)
{
    return 1.0 / (1.0 + GGX_Lambda_Aniso(a, incoming) + GGX_Lambda_Aniso(a, outgoing));
}

// GGX microfacet BRDF sampling function proportional to GGX distribution of visible normals:
// Sampling the GGX Distribution of Visible Normals
// jcgt.org/published/0007/04/01/paper.pdf
vec3 GGX_Aniso_Sample_Halfway(vec2 uv, vec2 a, vec3 incoming)
{
    // Translate from ellipsoid to the hemisphere
    vec3 m = normalize(vec3(a.x * incoming.x, incoming.y, a.y * incoming.z));

    // Section 4.1: orthonormal basis.
    // Here bias is still required.
    vec3 t1 = (m.y < 0.9999) ? normalize(cross(vec3(0.0, 1.0, 0.0), m)) : vec3(0.0, 0.0, 1.0);
    vec3 t2 = cross(m, t1);
    // Section 4.2: parameterization of the projected area.
    float r = sqrt(uv.x);
    float phi = 2.0 * EG_PI * uv.y;
    float a1 = r * cos(phi);
    float a2 = r * sin(phi);
    float s = 0.5 * (1.0 + m.y);
    a2 = (1.0 - s) * sqrt(1.0 - a1 * a1) + s * a2;
    // Section 4.3: reprojection onto hemisphere.
    vec3 mm = a1 * t1 + a2 * t2 + sqrt(max(0.0, 1.0 - a1 * a1 - a2 * a2)) * m;
    // Section 3.4: transforming the normal back to the ellipsoid configuration.
    m = normalize(vec3(a.x * mm.x, clamp(mm.y, 0.0, 1.0), a.y * mm.z));
    return m;
}

/*
 * Compute energy compensation lut
 */
vec2 GGX_Directional_Albedo_Importance_Sample(float NdotI, float roughness)
{
    NdotI = clamp(NdotI, FLT_EPSILON, 1.f);
    const vec3 incoming = vec3(sqrt(1.f - NdotI * NdotI), NdotI, 0);

    vec2 ab = vec2(0, 0);
    const int SAMPLE_COUNT = 1024;
    const float GOLDEN_RATIO = 1.6180339887;

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        const vec2 xi = vec2(fract((float(i) + 1.0) * GOLDEN_RATIO), (float(i) + 0.5) / float(SAMPLE_COUNT));
        const vec3 h = GGX_Aniso_Sample_Halfway(xi, vec2(roughness, roughness), incoming);
        const float VdotH = clamp(dot(incoming, h), FLT_EPSILON, 1.0);

        // Compute the Fresnel term.
        const float fc = pow(1.0 - VdotH, 5.0);

        const vec3 l = -reflect(incoming, h);
        // Compute the per-sample geometric term.
        const float GVis = GGX_G2_Aniso(vec2(roughness, roughness), incoming, l);

        // Add the contribution of this sample.
        ab += vec2(GVis * (1 - fc), GVis * fc);
    }

    // Normalize integrated terms.
    ab /= GGX_G1_Aniso(vec2(roughness, roughness), incoming) * SAMPLE_COUNT;

    return ab;
}

void main()
{		
    outColor = GGX_Directional_Albedo_Importance_Sample(i_UV.x, i_UV.y);
}
