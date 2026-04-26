#ifndef EG_LIGHT_UTILS
#define EG_LIGHT_UTILS

#include "common_structures.h"
#include "shadow_maps/shadows_utils.h"

vec3 CalculatePointLightRadiance(PointLight pointLight, vec3 worldPos, vec3 geometryNormal, vec3 shadingNormal, vec3 lambertAlbedo, vec3 V, vec3 F0,
    float metalness, float roughness, bool bInShadowRange)
{
    const vec3 incoming = pointLight.Position - worldPos;
    const float distance2 = dot(incoming, incoming);
    const bool bCastsShadows = (floatBitsToUint(pointLight.Radius2) & 0x80000000) != 0; // TODO: replace with `pointLight.Radius2 < 0.0`?
    const float radius2 = abs(pointLight.Radius2);
    if (distance2 > radius2)
    {
        return vec3(0);
    }

    const float attenuation = 1.f / distance2
        * EG_SQUARE(clamp(1.0 - EG_SQUARE(distance2 * 1.0f / radius2), 0.f, 1.f));

    const vec3 normIncoming = normalize(incoming);
    float shadow = 1.f;
#ifdef EG_TRANSLUCENT_SHADOWS
    vec3 coloredShadow = vec3(1.f);
#endif
    if (bCastsShadows)
    {
        if (bInShadowRange && NOT_ZERO(attenuation))
        {
            const uint shadowMapIndex = pointLight.ShadowMapIndex;
            if (shadowMapIndex < EG_MAX_LIGHT_SHADOW_MAPS)
            {
                const float NdotL = clamp(dot(normIncoming, geometryNormal), EG_FLT_SMALL, 1.0);
                shadow = PointLight_ShadowCalculation(g_PointShadowMaps[nonuniformEXT(shadowMapIndex)], -incoming, normIncoming, NdotL);
#ifdef EG_TRANSLUCENT_SHADOWS
                coloredShadow = PointLight_ColoredShadowCalculation(g_PointShadowMapsColored[nonuniformEXT(shadowMapIndex)], -incoming, geometryNormal, NdotL);
#endif
            }
        }
    }

    const vec3 pointLightLo = EvaluatePBR(lambertAlbedo, normIncoming, V, shadingNormal, F0, metalness, roughness, pointLight.LightColor, attenuation);
#ifdef EG_TRANSLUCENT_SHADOWS
    return pointLightLo * coloredShadow * shadow;
#else
    return pointLightLo * shadow;
#endif
}

vec3 CalculateSpotLightRadiance(SpotLight spotLight, vec3 worldPos, vec3 geometryNormal, vec3 shadingNormal, vec3 lambertAlbedo, vec3 V, vec3 F0,
    float metalness, float roughness, bool bInShadowRange)
{
    const vec3 incoming = spotLight.Position - worldPos;
    const float distance2 = dot(incoming, incoming);
    if (distance2 > spotLight.Distance2)
    {
        return vec3(0);
    }

    float attenuation = 1.f / distance2
        * EG_SQUARE(clamp(1.0 - EG_SQUARE(distance2 * 1.0f / spotLight.Distance2), 0.f, 1.f));

    const vec3 normIncoming = normalize(incoming);

    //Cutoff
    const float innerCutOffCos = cos(spotLight.InnerCutOffRadians);
    const float outerCutOffCos = cos(spotLight.OuterCutOffRadians);
    const float epsilon = innerCutOffCos - outerCutOffCos;
    const float theta = clamp(dot(normIncoming, normalize(-spotLight.Direction)), EG_FLT_SMALL, 1.0);
    const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);
    attenuation *= cutoffIntensity;

#ifdef EG_TRANSLUCENT_SHADOWS
    vec3 coloredShadow = vec3(1.f);
#endif
    float shadow = 1.f;
    if (spotLight.bCastsShadows != 0)
    {
        if (bInShadowRange && NOT_ZERO(attenuation))
        {
            const uint shadowMapIndex = spotLight.ShadowMapIndex;
            if (shadowMapIndex < EG_MAX_LIGHT_SHADOW_MAPS)
            {
                const mat4 viewProj = g_LightMatrices[spotLight.ViewProjOffset];
                const float NdotL = clamp(dot(normIncoming, geometryNormal), EG_FLT_SMALL, 1.0);
                const float texelSize = 1.f / textureSize(g_SpotShadowMaps[nonuniformEXT(shadowMapIndex)], 0).x;
                const float k = 20.f + (40.f * spotLight.OuterCutOffRadians * spotLight.OuterCutOffRadians) + distance2 * 2.2f; // Some magic number that help to fight against self-shadowing
                const float bias = texelSize * k;
                const vec3 normalBias = normIncoming * bias;
                vec4 lightSpacePos = viewProj * vec4(worldPos + normalBias, 1.0);
                lightSpacePos.xyz /= lightSpacePos.w;

#ifdef EG_TRANSLUCENT_SHADOWS
                coloredShadow = SpotLight_ColoredShadowCalculation(g_SpotShadowMapsColored[nonuniformEXT(shadowMapIndex)], lightSpacePos.xyz, NdotL);
#endif
                shadow = SpotLight_ShadowCalculation(g_SpotShadowMaps[nonuniformEXT(shadowMapIndex)], lightSpacePos.xyz, NdotL);
            }
        }
    }
    const vec3 spotLightLo = EvaluatePBR(lambertAlbedo, normIncoming, V, shadingNormal, F0, metalness, roughness, spotLight.LightColor, attenuation);
#ifdef EG_TRANSLUCENT_SHADOWS
    return spotLightLo * coloredShadow * shadow;
#else
    return spotLightLo * shadow;
#endif
}

vec3 CalculateDirectionalLightRadiance(DirectionalLight light, vec3 worldPos, vec3 geometryNormal, vec3 shadingNormal, vec3 lambertAlbedo, vec3 V, vec3 F0,
    float metalness, float roughness, bool bInShadowRange, mat4 view, float csmOverlap
#ifdef EG_ENABLE_CSM_VISUALIZATION
    , inout vec3 cascadeVisualizationColor
#endif
)
{
    const float cascadeDepth = abs((view * vec4(worldPos, 1.0)).z);
    int layer = GetCascadeIndex(light, cascadeDepth);

    const vec3 incoming = normalize(-light.Direction);
    float shadow = 1.f;
#ifdef EG_TRANSLUCENT_SHADOWS
    vec3 coloredShadow = vec3(1.f);
#endif
    if (layer != -1)
    {
#ifdef EG_ENABLE_CSM_VISUALIZATION
        const vec3 cascadeColors[EG_CASCADES_COUNT] = vec3[]
        (
            vec3(1, 0, 0),
            vec3(0, 1, 0),
            vec3(0, 0, 1),
            vec3(1, 0, 1)
            );
        cascadeVisualizationColor = cascadeColors[layer];
#endif // EG_ENABLE_CSM_VISUALIZATION
        if (light.bCastsShadows != 0 && bInShadowRange)
        {
            const float NdotL = clamp(dot(incoming, geometryNormal), EG_FLT_SMALL, 1.0);

            const float texelSize = 1.f / textureSize(g_DirShadowMaps[nonuniformEXT(layer)], 0).x;
            const float k = GetCascadeTexelOffset(layer);
            const float bias = texelSize * k;
            const vec3 normalBias = geometryNormal * bias;

            const mat4 viewProj = g_LightMatrices[light.ViewProjOffset + layer];
            const vec3 lightSpacePosBiased = (viewProj * vec4(worldPos + normalBias + incoming * bias * 1.5f, 1.0)).xyz;
            const vec3 lightSpacePos = (viewProj * vec4(worldPos, 1.0)).xyz;
            shadow = DirLight_ShadowCalculation(g_DirShadowMaps[nonuniformEXT(layer)], lightSpacePosBiased, NdotL, layer);
#ifdef EG_TRANSLUCENT_SHADOWS
            coloredShadow = DirLight_ColoredShadowCalculation(g_DirShadowMapsColored[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
#endif

#ifdef EG_CSM_SMOOTH_TRANSITION
            if (layer != EG_CASCADES_COUNT - 1)
            {
                const float currentSplit = light.CascadePlaneDistances[layer];
                const float nextSplit = currentSplit - currentSplit * csmOverlap;
                const float blendFactor = (cascadeDepth - nextSplit) / (currentSplit - nextSplit);
                if (blendFactor > 0.f)
                {
                    layer = layer + 1;
                    const float k = GetCascadeTexelOffset(layer);
                    const float texelSize = 1.f / textureSize(g_DirShadowMaps[nonuniformEXT(layer)], 0).x;
                    const float bias = texelSize * k;
                    const vec3 normalBias = geometryNormal * bias;
                    const mat4 nextViewProj = g_LightMatrices[light.ViewProjOffset + layer];

                    const vec3 lightSpacePosBiased = (nextViewProj * vec4(worldPos + normalBias + incoming * bias * 1.5f, 1.0)).xyz;
                    const vec3 lightSpacePos = (nextViewProj * vec4(worldPos, 1.0)).xyz;
                    const float nextShadow = DirLight_ShadowCalculation(g_DirShadowMaps[nonuniformEXT(layer)], lightSpacePosBiased, NdotL, layer);
                    shadow = mix(shadow, nextShadow, blendFactor);

#ifdef EG_TRANSLUCENT_SHADOWS
                    const vec3 nextColoredShadow = DirLight_ColoredShadowCalculation(g_DirShadowMapsColored[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
                    coloredShadow = mix(coloredShadow, nextColoredShadow, blendFactor);
#endif
                }
            }
#endif // EG_CSM_SMOOTH_TRANSITION
        }
    }
    const vec3 directional_Lo = EvaluatePBR(lambertAlbedo, incoming, V, shadingNormal, F0, metalness, roughness, light.LightColor, 1.f);
#ifdef EG_TRANSLUCENT_SHADOWS
    return directional_Lo * shadow * coloredShadow;
#else
    return directional_Lo * shadow;
#endif
}

#endif // EG_LIGHT_UTILS