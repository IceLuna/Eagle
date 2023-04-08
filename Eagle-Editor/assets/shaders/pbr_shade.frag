#include "pbr_pipeline_layout.h"
#include "utils.h"
#include "pbr_utils.h"
#include "shadows_utils.h"

#define EG_BASE_REFLECTIVITY 0.04f

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjInv;
    vec3 g_CameraPos;
    uint g_PointLightsCount;
    uint g_SpotLightsCount;
    uint g_HasDirectionalLight;
    float g_MaxReflectionLOD;
    uint g_HasIrradiance;
};

void main()
{
    const vec3 albedo = texture(g_AlbedoTexture, i_UV).rgb;
    const vec3 lambert_albedo = albedo * EG_INV_PI;
    const float depth = texture(g_DepthTexture, i_UV).x;
    const vec3 worldPos = WorldPosFromDepth(g_ViewProjInv, i_UV, depth);
    const vec3 shadingNormal = normalize(DecodeNormal(texture(g_ShadingNormalTexture, i_UV).xyz));
    const vec3 geometryNormal = normalize(DecodeNormal(texture(g_GeometryNormalTexture, i_UV).xyz));
    const vec3 emissive = texture(g_EmissiveTexture, i_UV).xyz;
    const vec4 materialData = texture(g_MaterialDataTexture, i_UV);
    
    const float metallness = materialData.x;
    const float roughness = materialData.y;
    const float ao = materialData.z;
    vec3 F0 = vec3(EG_BASE_REFLECTIVITY);
    F0 = mix(F0, albedo, metallness);
    const vec3 V = normalize(g_CameraPos - worldPos);

    vec3 Lo = vec3(0.f);

    // PointLights
    for (uint i = 0; i < g_PointLightsCount; ++i)
    {
        const PointLight pointLight = g_PointLights[i];
        vec3 incoming = pointLight.Position - worldPos;
        const float distance2 = dot(incoming, incoming);
	    const float attenuation = 1.f / distance2;
        incoming = normalize(incoming);
        const float NdotL = clamp(dot(incoming, geometryNormal), EG_FLT_SMALL, 1.0);
        
        const vec3 pointLightLo = EvaluatePBR(lambert_albedo, incoming, V, shadingNormal, F0, metallness, roughness, pointLight.LightColor, pointLight.Intensity * attenuation);
        const float shadow = i < EG_MAX_LIGHT_SHADOW_MAPS ? PointLight_ShadowCalculation(g_PointShadowMaps[i], -incoming, NdotL) : 1.f;
        Lo += pointLightLo * shadow;
    }

    // SpotLights
    for (uint i = 0; i < g_SpotLightsCount; ++i)
    {
        const SpotLight spotLight = g_SpotLights[i];
        vec3 incoming = spotLight.Position - worldPos;
        const float distance2 = dot(incoming, incoming);
	    const float attenuation = 1.f / distance2;
        incoming = normalize(incoming);

        //Cutoff
        const float theta = clamp(dot(incoming, normalize(-spotLight.Direction)), EG_FLT_SMALL, 1.0);
	    const float innerCutOffCos = cos(spotLight.InnerCutOffRadians);
	    const float outerCutOffCos = cos(spotLight.OuterCutOffRadians);
	    const float epsilon = innerCutOffCos - outerCutOffCos;
	    const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);

        const float NdotL = clamp(dot(incoming, geometryNormal), EG_FLT_SMALL, 1.0);
        vec4 lightSpacePos = spotLight.ViewProj * vec4(worldPos, 1.0);
        lightSpacePos.xyz /= lightSpacePos.w;
        const float shadow = i < EG_MAX_LIGHT_SHADOW_MAPS ? SpotLight_ShadowCalculation(g_SpotShadowMaps[i], lightSpacePos.xyz, NdotL) : 1.f;
        const vec3 spotLightLo = EvaluatePBR(lambert_albedo, incoming, V, shadingNormal, F0, metallness, roughness, spotLight.LightColor, spotLight.Intensity * cutoffIntensity * attenuation);
        Lo += spotLightLo * shadow;
    }

    // Directional light
#ifdef EG_ENABLE_CSM_VISUALIZATION
    vec3 cascadeVisualizationColor = vec3(1.f);
#endif
    if (g_HasDirectionalLight > 0)
    {
        const float cascadeDepth = abs((g_CameraView * vec4(worldPos, 1.0)).z);
        int layer = -1;
        for (int i = 0; i < EG_CASCADES_COUNT; ++i)
        {
            if (cascadeDepth < g_DirectionalLight.CascadePlaneDistances[i])
            {
                layer = i;
                break;
            }
        }

        const vec3 incoming = normalize(-g_DirectionalLight.Direction);
        vec3 directional_Lo = EvaluatePBR(lambert_albedo, incoming, V, shadingNormal, F0, metallness, roughness, g_DirectionalLight.LightColor, g_DirectionalLight.Intensity);
        float shadow = 1.f;
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
#endif
            const float NdotL = clamp(dot(incoming, geometryNormal), EG_FLT_SMALL, 1.0);
            const vec4 lightSpacePos = g_DirectionalLight.ViewProj[layer] * vec4(worldPos, 1.0);
            shadow = DirLight_ShadowCalculation(g_DirShadowMaps[nonuniformEXT(layer)], lightSpacePos.xyz, NdotL);
        }
        Lo += directional_Lo * shadow;
    }

    // Ambient
    vec3 ambient = vec3(0.f);
    if (g_HasIrradiance > 0)
    {
        const float NdotV = clamp(dot(shadingNormal, V), EG_FLT_SMALL, 1.0);
        const vec3 irradiance = texture(g_IrradianceMap, shadingNormal).rgb;
        const vec3 R = reflect(-V, shadingNormal);
        const vec3 radiance = textureLod(g_PrefilterMap, R, roughness * g_MaxReflectionLOD).rgb;
        const vec2 envBRDF = texture(g_BRDFLUT, vec2(NdotV, roughness)).rg;

        const vec3 diffuseColor = albedo * (1.f - EG_BASE_REFLECTIVITY) * (1.f - metallness);
        const vec3 Fr = max(vec3(1.f - roughness), F0) - F0;
        const vec3 kS = F0 + Fr * pow(1.f - NdotV, 5.f);

        const vec3 FssEss = kS * envBRDF.x + envBRDF.y;

        // Multiple scattering, from Fdez-Aguera
        const float Ems = (1.0 - (envBRDF.x + envBRDF.y));
        const vec3 Favg = F0 + (1.0 - F0) / 21.0;
        const vec3 FmsEms = Ems * FssEss * Favg / (1.0 - Favg * Ems);
        const vec3 kD = diffuseColor * (1.0 - FssEss - FmsEms);
        const vec3 color = FssEss * radiance + (FmsEms + kD) * irradiance;
        ambient = color * ao;
    }

    vec3 resultColor = ambient + Lo + emissive;
#ifdef EG_ENABLE_CSM_VISUALIZATION
    resultColor += cascadeVisualizationColor;
#endif

    outColor = vec4(resultColor, 1.f);
}
