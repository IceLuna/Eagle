// Loop32 was used from https://github.com/nvpro-samples/vk_order_independent_transparency
// Minor modification were made to the color & composite algorithms to store HDR colors

#include "pipeline_layout.h"
#include "utils.h"
#include "transparency/transparency_color_pipeline_layout.h"

#define PBR_EVALUATE_IBL
#include "pbr_utils.h"
#define EG_OIT_NULL 0x0u // 0xFFFFFFFFu

#define EG_PIXEL_COORDS vec2(gl_FragCoord.xy)
#include "light_utils.h"
#include "postprocessing/utils.h"

// Input
layout(location = 0) in vec3 i_Normal;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in uint i_MaterialIndex;
layout(location = 3) in vec3 i_WorldPos;
layout(location = 4) in mat3 i_TBN;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const bool s_HasIrradiance = false;

layout(set = 5, binding = 0, r32ui) uniform coherent uimageBuffer imgAbuffer;

layout(set = 5, binding = 1)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

layout(set = 5, binding = 2) uniform UniformBuffer
{
    vec3 g_CameraPos;
    float g_MaxReflectionLOD;
    ivec2 g_Size;
    float g_CSMOverlap;
    float g_IBLIntensity;
    uint g_TilesBufferWidth;
    uint g_HasDirLight;
};

#ifdef EG_FOG
layout(set = 5, binding = 3) uniform FogData
{
    vec3  g_FogColor;
    float g_FogMin;
    float g_FogMax;
    float g_FogDensity;
    uint  g_FogEquation;
};
#endif

#extension GL_ARB_post_depth_coverage : enable
layout(post_depth_coverage) in;

// For each fragment, we look up its depth in the sorted array of depths.
// If we find a match, we write the fragment's color into the corresponding
// place in an array of colors. Otherwise, we tail blend it if enabled.

vec3 Lighting(in ShaderMaterial material, vec2 uv);

void main()
{
    vec2 uv = i_TexCoords;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);

    vec4 color = vec4(vec3(0.f), material.Opacity);
    if (!IS_ZERO(material.Opacity))
        color.rgb = Lighting(material, uv);

#ifdef EG_FOG
    {
        const float fogDistance = length(i_WorldPos - g_CameraPos);
        const float fogAlpha = GetFogFactor(g_FogEquation, fogDistance, g_FogDensity, g_FogMin, g_FogMax);
        color.rgb = mix(color.rgb, g_FogColor, fogAlpha);  
    }
#endif

    // Compute base index in the A-buffer
    const int viewSize = g_Size.x * g_Size.y;
    ivec2 coord = ivec2(gl_FragCoord.xy);
    const int listPos = coord.y * g_Size.x + coord.x;

    const uint zcur = floatBitsToUint(gl_FragCoord.z);

    // If this fragment was behind the frontmost EG_OIT_LAYERS fragments, it didn't
    // make it in, so tail blend it:
    if(imageLoad(imgAbuffer, listPos + (EG_OIT_LAYERS - 1) * viewSize).x > zcur)
    {
        outColor = vec4(color.rgb * color.a, color.a); // TAILBLEND
        return;
    }

    // Use binary search to determine which index this depth value corresponds to
    // At each step, we know that it'll be in the closed interval [start, end].
    int start = 0;
    int end = (EG_OIT_LAYERS - 1);
    uint ztest;
    while(start < end)
    {
        int mid = (start + end) / 2;
        ztest = imageLoad(imgAbuffer, listPos + mid * viewSize).x;
        if(ztest > zcur)
            start = mid + 1;  // in [mid + 1, end]
        else
            end = mid;  // in [start, mid]
    }

    // We now have start == end. Insert the packed color into the A-buffer at
    // this index.
    //imageStore(imgAbuffer, listPos + (EG_OIT_LAYERS + start) * viewSize, uvec4(packUnorm4x8(color)));
    
    imageStore(imgAbuffer, listPos + (EG_OIT_LAYERS + start) * viewSize, uvec4(packHalf2x16(color.rg)));
    imageStore(imgAbuffer, listPos + (EG_OIT_LAYERS * 2 + start) * viewSize, uvec4(packHalf2x16(color.ba)));

    // Inserted, so make this color transparent:
    outColor = vec4(0);
}

vec3 Lighting(in ShaderMaterial material, vec2 uv)
{
    const vec3 albedo = material.Albedo;
    const vec3 lambert_albedo = albedo * EG_INV_PI;
    const vec3 worldPos = i_WorldPos;
    const vec3 geometryNormal = normalize(i_Normal);

    const float metalness = material.Metalness;
    const float ao = material.AO;
	const float roughness = ApplyGeometricSpecularAntiAliasing(geometryNormal, material.Roughness);
    const vec3 F0 = mix(vec3(EG_BASE_REFLECTIVITY), albedo, metalness);

    const vec3 fragToCamera = g_CameraPos - worldPos;
    const vec3 V = normalize(fragToCamera);

    vec3 Lo = vec3(0.f);
    vec3 shadingNormal = geometryNormal;
    if (material.NormalTextureIndex != EG_INVALID_INDEX)
    {
        shadingNormal = ReadTexture(material.NormalTextureIndex, uv).rgb;
        shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
        shadingNormal = normalize(i_TBN * shadingNormal);
    }

    const uvec2 tileID = uvec2(gl_FragCoord.xy) / EG_LIGHT_CULLING_TILE_SIZE;
    const uint tileIndex = tileID.x + tileID.y * g_TilesBufferWidth;
    const uint bucketStartIndex = tileIndex * EG_LIGHTS_BUCKET_COUNT;

    // PointLights
    const uint pointLightsBucketsCount = g_PointLightsCount > 0 ? min(EG_LIGHTS_BUCKET_COUNT, ((g_PointLightsCount - 1) / EG_LIGHT_BUCKET_SIZE) + 1) : 0u;
    for (uint bucket = 0; bucket < pointLightsBucketsCount; bucket++)
    {
        uint bucketBits = g_PointLightsBuckets[bucketStartIndex + bucket];
        while(bucketBits > 0)
        {
            const uint bucketLightIndex = findLSB(bucketBits);
            const uint lightIndex = EG_LIGHT_BUCKET_SIZE * bucket + bucketLightIndex;
            bucketBits ^= (1 << bucketLightIndex);

            const PointLight pointLight = g_PointLights[lightIndex];
            Lo += CalculatePointLightRadiance(pointLight, worldPos, geometryNormal, shadingNormal, lambert_albedo, V, F0, metalness, roughness);
        }
    }

    // SpotLights
    const uint spotLightsBucketsCount = g_SpotLightsCount > 0 ? min(EG_LIGHTS_BUCKET_COUNT, ((g_SpotLightsCount - 1) / EG_LIGHT_BUCKET_SIZE) + 1) : 0u;
    for (uint bucket = 0; bucket < spotLightsBucketsCount; bucket++)
    {
        uint bucketBits = g_SpotLightsBuckets[bucketStartIndex + bucket];
        while(bucketBits > 0)
        {
            const uint bucketLightIndex = findLSB(bucketBits);
            const uint lightIndex = EG_LIGHT_BUCKET_SIZE * bucket + bucketLightIndex;
            bucketBits ^= (1 << bucketLightIndex);

            const SpotLight spotLight = g_SpotLights[lightIndex];
            Lo += CalculateSpotLightRadiance(spotLight, worldPos, geometryNormal, shadingNormal, lambert_albedo, V, F0, metalness, roughness);
        }
    }

    // Directional light
#ifdef EG_ENABLE_CSM_VISUALIZATION
    vec3 cascadeVisualizationColor = vec3(0.f);
#endif

    if (g_HasDirLight != 0)
    {
        Lo += CalculateDirectionalLightRadiance(g_DirectionalLight, worldPos, geometryNormal, shadingNormal, lambert_albedo, V, F0, metalness, roughness,
            g_View, g_CSMOverlap
#ifdef EG_ENABLE_CSM_VISUALIZATION
            , cascadeVisualizationColor
#endif
        );
    }

    // Ambient
    vec3 ambient = (g_HasDirLight != 0) ? (albedo * g_DirectionalLight.Ambient) : vec3(0.f);
    if (s_HasIrradiance)
    {
        ambient += EvaluateIBL(albedo, F0, shadingNormal, V, roughness, metalness, g_MaxReflectionLOD) * ao * g_IBLIntensity;
    }

    const vec3 emissive = material.Emissive;
    vec3 resultColor = ambient + Lo + emissive;
#ifdef EG_ENABLE_CSM_VISUALIZATION
    resultColor += cascadeVisualizationColor;
#endif

    return resultColor;
}
