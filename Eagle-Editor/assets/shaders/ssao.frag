#include "utils.h"

layout(location = 0) in vec2 i_UV;
layout(location = 0) out float outColor;

layout(push_constant) uniform PushConstants
{
    mat4 g_ProjMat;
    vec3 g_ViewRow1;
    uint g_SamplesSize;
    vec3 g_ViewRow2;
    float g_Radius;
    vec3 g_ViewRow3;
    float g_Bias;

    vec2 g_NoiseScale;
};

layout(binding = 0) uniform sampler2D g_Albedo;
layout(binding = 1) uniform sampler2D g_Normal;
layout(binding = 2) uniform sampler2D g_Depth;
layout(binding = 3) uniform sampler2D g_Noise;
layout(binding = 4) buffer SamplesBuffer
{
    vec3 g_Samples[];
};

void main()
{
    const mat4 invProj = inverse(g_ProjMat);
    const mat3 viewMat = mat3(g_ViewRow1, g_ViewRow2, g_ViewRow3);

    const vec3 albedo = texture(g_Albedo, i_UV).rgb;
    const float depth = texture(g_Depth, i_UV).x;
    const vec3 viewPos = ViewPosFromDepth(invProj, i_UV, depth);
    const vec3 viewNormal = viewMat * normalize(DecodeNormal(texture(g_Normal, i_UV).xyz));
    const vec3 randomVec = vec3(texture(g_Noise, i_UV * g_NoiseScale).xy, 0.f);
    
    const vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    const vec3 bitangent = cross(viewNormal, tangent);
    const mat3 TBN = mat3(tangent, bitangent, viewNormal);
    
    float occlusion = 0.f;
    for (uint i = 0; i < g_SamplesSize; ++i)
    {
        vec3 samplePos = TBN * g_Samples[i]; // From tangent to View-space
        samplePos = viewPos + samplePos * g_Radius;
        vec4 clipSpacePos = g_ProjMat * vec4(samplePos, 1.0);
        clipSpacePos /= clipSpacePos.w;
        clipSpacePos.xy = clipSpacePos.xy * 0.5f + 0.5f;
    
        float sampleDepth = texture(g_Depth, clipSpacePos.xy).x;
        sampleDepth = ViewPosFromDepth(invProj, clipSpacePos.xy, sampleDepth).z;
    
        float rangeCheck = smoothstep(0.0, 1.0, g_Radius / abs(viewPos.z - sampleDepth));
        occlusion += (sampleDepth >= (samplePos.z + g_Bias) ? 1.0 : 0.0) * rangeCheck;
    }
    
    outColor = 1.f - (occlusion / float(g_SamplesSize));
}
