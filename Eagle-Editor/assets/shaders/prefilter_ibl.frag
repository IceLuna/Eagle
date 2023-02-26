#include "defines.h"
#include "pbr_utils.h"

layout(location = 0) in vec3 i_Pos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_Cubemap;

layout(push_constant) uniform PushData
{
    layout(offset = 64) float g_Roughness;
    layout(offset = 68) uint g_ResPerFace;
};

void main()
{		
    const vec3 N = normalize(i_Pos);

    // make the simplyfying assumption that V equals R equals the normal 
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024;
    vec3 prefilteredColor = vec3(0.f);
    float totalWeight = 0.0;

    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, g_Roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            // sample from the environment's mip level based on roughness/pdf
            float D   = DistributionGGX(N, H, g_Roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001f; 

            float saTexel  = 4.0 * EG_PI / (6.f * g_ResPerFace * g_ResPerFace);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = g_Roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
            
            prefilteredColor += textureLod(u_Cubemap, L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    outColor = vec4(prefilteredColor, 1.0);
}
