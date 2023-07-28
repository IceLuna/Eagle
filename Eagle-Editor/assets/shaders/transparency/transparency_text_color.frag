// Loop32 was used from https://github.com/nvpro-samples/vk_order_independent_transparency
// Minor modification were made to the color & composite algorithms to store HDR colors

#extension GL_EXT_nonuniform_qualifier : enable

#include "utils.h"
#include "transparency/transparency_color_pipeline_layout.h"

#include "pbr_utils.h"

#define EG_PIXEL_COORDS vec2(gl_FragCoord.xy)
#include "shadows_utils.h"

// Input
layout(location = 0) in vec4      i_AlbedoRoughness;
layout(location = 1) in vec4      i_EmissiveMetallness;
layout(location = 2) in vec3      i_Normal;
layout(location = 3) in float     i_AO;
layout(location = 4) in vec3      i_WorldPos;
layout(location = 5) flat in uint i_AtlasIndex;
layout(location = 6) in vec2      i_TexCoords;
layout(location = 7) in float     i_Opacity;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) vec3 g_CameraPos;
    float g_MaxReflectionLOD;
    ivec2 g_Size;
    float g_MaxShadowDistance2;
};

layout(binding = 1, r32ui) uniform coherent uimageBuffer imgAbuffer;

layout(binding = 2)
uniform CameraView
{
	mat4 g_CameraView;
};

layout(set = 2, binding = 0) uniform sampler2D g_FontAtlases[EG_MAX_TEXTURES];

#extension GL_ARB_post_depth_coverage : enable
layout(post_depth_coverage) in;

// For each fragment, we look up its depth in the sorted array of depths.
// If we find a match, we write the fragment's color into the corresponding
// place in an array of colors. Otherwise, we tail blend it if enabled.

vec4 Lighting();

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange()
{
	const float pxRange = 2.0f;
    const vec2 unitRange = vec2(pxRange) / vec2(textureSize(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(i_TexCoords);
    return max(0.5f * dot(unitRange, screenTexSize), 1.0);
}

void main()
{
    const vec3 msd = texture(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords).rgb;
    const float sd = median(msd.r, msd.g, msd.b);
    const float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    const float fadeoutOpacity = clamp(screenPxDistance + 0.5f, 0.f, 1.f);

	if (IS_ZERO(fadeoutOpacity))
    {
		discard;
        return;
    }

    vec4 color = Lighting();

    // Compute base index in the A-buffer
    const int viewSize = g_Size.x * g_Size.y;
    ivec2 coord = ivec2(gl_FragCoord.xy);
    const int listPos = coord.y * g_Size.x + coord.x;

    const uint zcur = floatBitsToUint(gl_FragCoord.z);

    // If this fragment was behind the frontmost EG_OIT_LAYERS fragments, it didn't
    // make it in, so tail blend it:
    if(imageLoad(imgAbuffer, listPos + (EG_OIT_LAYERS - 1) * viewSize).x < zcur)
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
        if(ztest < zcur)
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

layout(constant_id = 0) const uint s_PointLights = 0;
layout(constant_id = 1) const uint s_SpotLights = 0;
layout(constant_id = 2) const bool s_HasDirLight = false;
layout(constant_id = 3) const bool s_HasIrradiance = false;

vec4 Lighting()
{
    const vec4 albedo_roughness = i_AlbedoRoughness;
    const vec3 lambert_albedo = albedo_roughness.rgb * EG_INV_PI;
    const vec3 worldPos = i_WorldPos;

    const float metallness = i_EmissiveMetallness.a;
    const float ao = i_AO;
    const float roughness = albedo_roughness.a;
    const vec3 F0 = mix(vec3(EG_BASE_REFLECTIVITY), albedo_roughness.rgb, metallness);

    const vec3 fragToCamera = g_CameraPos - worldPos;
    const bool bInShadowRange = dot(fragToCamera, fragToCamera) < g_MaxShadowDistance2;
    const vec3 V = normalize(fragToCamera);

    vec3 Lo = vec3(0.f);
    const vec3 normal = normalize(i_Normal);

    // PointLights
    uint plShadowMapIndex = 0;
    for (uint i = 0; i < s_PointLights; ++i)
    {
        const PointLight pointLight = g_PointLights[i];
        const vec3 incoming = pointLight.Position - worldPos;
	    const float distance2 = dot(incoming, incoming);
        const bool bCastsShadows = (floatBitsToUint(pointLight.Intensity) & 0x80000000) != 0; // TODO: replace with `pointLight.Intensity < 0.0`
        if (distance2 > (pointLight.Radius * pointLight.Radius))
        {
            if (bCastsShadows)
                plShadowMapIndex++;
            continue;
        }
	    const float attenuation = 1.f / distance2;
        const float totalIntensity = abs(pointLight.Intensity) * attenuation;

        const vec3 normIncoming = normalize(incoming);
        float shadow = 1.f;
        if (bCastsShadows && bInShadowRange)
        {
            const float NdotL = clamp(dot(normIncoming, normal), EG_FLT_SMALL, 1.0);

            shadow = plShadowMapIndex < EG_MAX_LIGHT_SHADOW_MAPS ? PointLight_ShadowCalculation(g_PointShadowMaps[plShadowMapIndex], -incoming, normal, NdotL) : 1.f;
            plShadowMapIndex++;
        }

        const vec3 pointLightLo = EvaluatePBR(lambert_albedo, normIncoming, V, normal, F0, metallness, roughness, pointLight.LightColor, totalIntensity);
        Lo += pointLightLo * shadow;
    }

    // SpotLights
    uint slShadowMapIndex = 0;
    for (uint i = 0; i < s_SpotLights; ++i)
    {
        const SpotLight spotLight = g_SpotLights[i];
        const vec3 incoming = spotLight.Position - worldPos;
	    const float distance2 = dot(incoming, incoming);
        if (distance2 > (spotLight.Distance * spotLight.Distance))
        {
            if (spotLight.bCastsShadows != 0)
                slShadowMapIndex++;
            continue;
        }

	    const float attenuation = 1.f / distance2;
        float totalIntensity = spotLight.Intensity * attenuation;

        const vec3 normIncoming = normalize(incoming);

        //Cutoff
	    const float innerCutOffCos = cos(spotLight.InnerCutOffRadians);
	    const float outerCutOffCos = cos(spotLight.OuterCutOffRadians);
	    const float epsilon = innerCutOffCos - outerCutOffCos;
        const float theta = clamp(dot(normIncoming, normalize(-spotLight.Direction)), EG_FLT_SMALL, 1.0);
	    const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);
        totalIntensity *= cutoffIntensity;

        float shadow = 1.f;
        if (spotLight.bCastsShadows != 0 && bInShadowRange)
        {
            const float NdotL = clamp(dot(normIncoming, normal), EG_FLT_SMALL, 1.0);

            const float texelSize = 1.f / textureSize(g_SpotShadowMaps[slShadowMapIndex], 0).x;
	        const float k = 20.f + (40.f * spotLight.OuterCutOffRadians) + distance2 * 2.2f; // Some magic number that help to fight against self-shadowing
	        const float bias = texelSize * k;
	        const vec3 normalBias = normal * bias;
            vec4 lightSpacePos = spotLight.ViewProj * vec4(worldPos + normalBias, 1.0);
            lightSpacePos.xyz /= lightSpacePos.w;

            shadow = slShadowMapIndex < EG_MAX_LIGHT_SHADOW_MAPS ? SpotLight_ShadowCalculation(g_SpotShadowMaps[slShadowMapIndex], lightSpacePos.xyz, NdotL) : 1.f;
            slShadowMapIndex++;
        }
        const vec3 spotLightLo = EvaluatePBR(lambert_albedo, normIncoming, V, normal, F0, metallness, roughness, spotLight.LightColor, totalIntensity);
        Lo += spotLightLo * shadow;
    }

    // Directional light
#ifdef EG_ENABLE_CSM_VISUALIZATION
    vec3 cascadeVisualizationColor = vec3(0.f);
#endif

    if (s_HasDirLight)
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
#endif // EG_ENABLE_CSM_VISUALIZATION
            if (g_DirectionalLight.bCastsShadows != 0 && bInShadowRange)
            {
                const float NdotL = clamp(dot(incoming, normal), EG_FLT_SMALL, 1.0);

            	const float texelSize = 1.f / textureSize(g_DirShadowMaps[nonuniformEXT(layer)], 0).x;
                const float k = 100.f;
                const float bias = texelSize * k;
                const vec3 normalBias = normal * bias;

                const vec3 lightSpacePos = (g_DirectionalLight.ViewProj[layer] * vec4(worldPos + normalBias, 1.0)).xyz;
                shadow = DirLight_ShadowCalculation(g_DirShadowMaps[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);

#ifdef EG_CSM_SMOOTH_TRANSITION
                if (layer != EG_CASCADES_COUNT - 1)
                {
                    const float overlap = float(EG_CSM_OVERLAP_PERCENT) / 100.f;
                    const float currentSplit = g_DirectionalLight.CascadePlaneDistances[layer];
                    const float nextSplit = currentSplit - currentSplit * overlap;
                    const float blendFactor = (cascadeDepth - nextSplit) / (currentSplit - nextSplit);
                    if (blendFactor > 0.f)
                    {
                        layer = layer + 1;
                        const float texelSize = 1.f / textureSize(g_DirShadowMaps[nonuniformEXT(layer)], 0).x;
                        const float bias = texelSize * k;
                        const vec3 normalBias = normal * bias;
                        const vec3 lightSpacePos = (g_DirectionalLight.ViewProj[layer] * vec4(worldPos + normalBias, 1.0)).xyz;
                        const float nextShadow = DirLight_ShadowCalculation(g_DirShadowMaps[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
                    
                        shadow = mix(shadow, nextShadow, blendFactor);
                    }
                }
#endif // EG_CSM_SMOOTH_TRANSITION
            }
        }
        const vec3 directional_Lo = EvaluatePBR(lambert_albedo, incoming, V, normal, F0, metallness, roughness, g_DirectionalLight.LightColor, g_DirectionalLight.Intensity);
        Lo += directional_Lo * shadow;
    }

    // Ambient
    vec3 ambient = vec3(0.f);
    if (s_HasIrradiance)
    {
        const vec3 R = reflect(-V, normal);
        const float NdotV = clamp(dot(normal, V), EG_FLT_SMALL, 1.0);

        const vec3 Fr = max(vec3(1.f - roughness), F0) - F0;
        const vec3 kS = F0 + Fr * pow(1.f - NdotV, 5.f);

        const vec2 envBRDF = texture(g_BRDFLUT, vec2(NdotV, roughness)).rg;
        const vec3 FssEss = kS * envBRDF.x + envBRDF.y;

        // Multiple scattering, from Fdez-Aguera
        const float Ems = (1.0 - (envBRDF.x + envBRDF.y));
        const vec3 Favg = F0 + (1.0 - F0) / 21.0;
        const vec3 FmsEms = Ems * FssEss * Favg / (1.0 - Favg * Ems);

        const vec3 diffuseColor = albedo_roughness.rgb * (1.f - EG_BASE_REFLECTIVITY) * (1.f - metallness);
        const vec3 kD = diffuseColor * (1.0 - FssEss - FmsEms);

        const vec3 radiance = textureLod(g_PrefilterMap, R, roughness * g_MaxReflectionLOD).rgb;
        const vec3 irradiance = texture(g_IrradianceMap, normal).rgb;
        const vec3 color = FssEss * radiance + (FmsEms + kD) * irradiance;
        ambient = color * ao;
    }

    const vec3 emissive = i_EmissiveMetallness.rgb;
    vec3 resultColor = ambient + Lo + emissive;
#ifdef EG_ENABLE_CSM_VISUALIZATION
    resultColor += cascadeVisualizationColor;
#endif

    const float opacity = i_Opacity;
    return vec4(resultColor, opacity);
}
