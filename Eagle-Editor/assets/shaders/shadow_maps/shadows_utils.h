#ifndef EG_SHADOWS_UTILS
#define EG_SHADOWS_UTILS

#include "defines.h"

const float s_BaseBias = 0.000001;

int GetCascadeIndex(in DirectionalLight light, float cascadeDepth)
{
	for (int i = 0; i < EG_CASCADES_COUNT; ++i)
	{
		if (cascadeDepth < light.CascadePlaneDistances[i])
			return i;
	}
	return -1;
}

float GetCascadeNormalOffsetScale(uint cascade)
{
	switch (cascade)
	{
		case 0: return 30.f;
		case 1: return 50.f;
		case 2: return 95.f;
		case 3: return 275.f;
	}
	return 100.f;
}

vec3 GetPointLightSamplePosition(samplerCubeShadow shadowMap, PointLight light, vec3 worldPos, vec3 geomNormal, float NdotL, float receiverDistance)
{
	vec3 normalOffset = vec3(0);
	if (NdotL > 0)
	{
		const float shadowMapSize = float(textureSize(shadowMap, 0).x);
		const float texelWorldSize = receiverDistance / shadowMapSize;
		const float normalBiasScale = mix(0.5, 2.0, 1.0 - NdotL);

		normalOffset = geomNormal * texelWorldSize * normalBiasScale * 2.0;
	}

	return (worldPos + normalOffset) - light.Position;
}

// Applies normal offset
vec3 GetSpotLightSpacePosition(sampler2DShadow shadowMap, SpotLight light, mat4 lightVP, vec3 worldPos, vec3 geomNormal, vec3 normIncoming, float receiverDistance, out float lightRadiusUV)
{
	lightRadiusUV = 0;

	float NdotL = dot(geomNormal, normIncoming);
	vec3 normalOffset = vec3(0);
	if (NdotL > 0)
	{
		// Approximate spotlight frustum width at receiver distance
		const float shadowMapSize = float(textureSize(shadowMap, 0).x);
		const float frustumWidth = 2.0 * tan(light.OuterCutOffRadians) * receiverDistance;
		const float texelWorldSize = frustumWidth / shadowMapSize;
		const float normalBiasScale = mix(0.5, 2.0, 1.0 - NdotL);

		// Texel scaled normal bias
		normalOffset = geomNormal * texelWorldSize * normalBiasScale * 2.0;

		const float baseDiskRadius = 0.0001f;
		lightRadiusUV = max(10.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);

		// Constant scale (2.5)
		//lightRadiusUV = 2.5 / shadowMapSize;
	}

	vec4 lightSpacePos = lightVP * vec4(worldPos + normalOffset, 1.0);
	lightSpacePos.xyz /= lightSpacePos.w;

	return lightSpacePos.xyz;
}

vec3 GetDirectionalLightSamplePosition(sampler2DShadow shadowMap, mat4 lightVP, vec3 worldPos, vec3 geomNormal, vec3 normIncoming, uint cascade, out float lightRadiusUV)
{
	lightRadiusUV = 0;

	float NdotL = dot(geomNormal, normIncoming);
	vec3 normalOffset = vec3(0);
	if (NdotL > 0)
	{
		// Approximate spotlight frustum width at receiver distance
		const float shadowMapSize = float(textureSize(shadowMap, 0).x);
		const float texelWorldSize = 1 / shadowMapSize;
		const float normalBiasScale = mix(0.5, 2.0, 1.0 - NdotL);

		// Texel scaled normal bias
		normalOffset = geomNormal * texelWorldSize * normalBiasScale * GetCascadeNormalOffsetScale(cascade);

		//const float baseDiskRadius = 0.0001f;
		//lightRadiusUV = max(10.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);

		// Constant scale
		lightRadiusUV = 1.0 / shadowMapSize;
	}

	vec4 lightSpacePos = lightVP * vec4(worldPos + normalOffset, 1.0);
	lightSpacePos.xyz /= lightSpacePos.w;

	return lightSpacePos.xyz;
}

vec3 GetDirectionalLightSamplePosition(sampler2DShadow shadowMap, mat4 lightVP, vec3 worldPos, vec3 geomNormal, vec3 normIncoming, uint cascade)
{
	float lightRadiusUV = 0.0;
	return GetDirectionalLightSamplePosition(shadowMap, lightVP, worldPos, geomNormal, normIncoming, cascade, lightRadiusUV);
}

// Applies normal offset
vec3 GetSpotLightSpacePosition(sampler2DShadow shadowMap, SpotLight light, mat4 lightVP, vec3 worldPos, vec3 geomNormal, vec3 normIncoming, float receiverDistance)
{
	float lightRadiusUV = 0;
	return GetSpotLightSpacePosition(shadowMap, light, lightVP, worldPos, geomNormal, normIncoming, receiverDistance, lightRadiusUV);
}

float ShadowVisibility_PCF(sampler2DShadow shadowMap, vec2 uv, float currentDepth, int pcfSize)
{
	const int pcfRange = pcfSize / 2;
	const float invPCFMatrixSize = 1.f / (pcfSize * pcfSize);

	const float texelSize = 1.0 / float(textureSize(shadowMap, 0).x);
	float visibility = 0.f;
	for (int x = -pcfRange; x <= pcfRange; ++x)
		for (int y = -pcfRange; y <= pcfRange; ++y)
		{
			const vec2 uv = uv + vec2(x, y) * texelSize;
			visibility += texture(shadowMap, vec3(uv, currentDepth));
		}

	return visibility * invPCFMatrixSize;
}

float ShadowVisibility_PCF(sampler2DShadow shadowMap, vec2 uv, float currentDepth)
{
	const int pcfSize = 3;
	return ShadowVisibility_PCF(shadowMap, uv, currentDepth, pcfSize);
}

#ifdef EG_SOFT_SHADOWS

float DirLight_ShadowCalculation_Soft(sampler2DShadow shadowMap, vec3 fragPosLightSpace, float lightRadiusUV, int cascade)
{
	const float bias = s_BaseBias;
	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	const ivec2 pixel = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	float visibilitySum = 0.0;
	uint samples = 0;

	const int earlySamples = 8;
	for (int i = 0; i < earlySamples / 2; ++i)
	{
		const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0);
		const vec2 offset0 = offsets.xy * lightRadiusUV;
		const vec2 offset1 = offsets.zw * lightRadiusUV;

		visibilitySum += texture(shadowMap, vec3(uv + offset0, currentDepth));
		visibilitySum += texture(shadowMap, vec3(uv + offset1, currentDepth));

		samples += 2;
	}

	float visibility = visibilitySum / samples;

	if (visibility > 0.0 && visibility < 1.0)
	{
		const int totalSamples = EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE;
		for (int i = earlySamples / 2; i < totalSamples / 2; ++i)
		{
			const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0);
			const vec2 offset0 = offsets.xy * lightRadiusUV;
			const vec2 offset1 = offsets.zw * lightRadiusUV;

			visibilitySum += texture(shadowMap, vec3(uv + offset0, currentDepth));
			visibilitySum += texture(shadowMap, vec3(uv + offset1, currentDepth));

			samples += 2;
		}

		visibilitySum /= float(samples);
	}

	return visibility;
}

float PointLight_ShadowCalculation_Soft(samplerCubeShadow depthTexture, vec3 samplePos, float NdotL, float farDistance)
{
	const float bias = s_BaseBias;
	const float currentDepth = VectorToDepth(samplePos, farDistance, EG_POINT_LIGHT_NEAR) + bias;
	const ivec2 pixel = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	float visibilitySum = 0.f;
	uint samples = 0;

	const float baseDiskRadius = 0.0001f;
	const float diskRadius = max(10.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);
	for (int i = 0; i < 4; ++i)
	{
		const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
		const vec3 offset0 = offsets.xyz * diskRadius;
		const vec3 offset1 = offsets.wxz * diskRadius;

		visibilitySum += texture(depthTexture, vec4(samplePos + offset0, currentDepth)).r;
		visibilitySum += texture(depthTexture, vec4(samplePos + offset1, currentDepth)).x;

		samples += 2;
	}
	float visibility = visibilitySum / samples;

	if (visibility > 0.0 && visibility < 1.0)
	{
		const int totalSamples = EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE;
		for (int i = 4; i < totalSamples / 2; ++i)
		{
			const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
			const vec3 offset0 = offsets.xyz * diskRadius;
			const vec3 offset1 = offsets.wxz * diskRadius;

			visibilitySum += texture(depthTexture, vec4(samplePos + offset0, currentDepth)).r;
			visibilitySum += texture(depthTexture, vec4(samplePos + offset1, currentDepth)).x;

			samples += 2;
		}

		visibility = visibilitySum / samples;
	}

	return visibility;
}

float SpotLight_ShadowCalculation_Soft(sampler2DShadow shadowMap, vec3 fragPosLightSpace, float lightRadiusUV)
{
	const float bias = s_BaseBias;
	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	const ivec2 pixel = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	float visibilitySum = 0.0;
	uint samples = 0;

	const int earlySamples = 8;
	for (int i = 0; i < earlySamples / 2; ++i)
	{
		const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0);
		const vec2 offset0 = offsets.xy * lightRadiusUV;
		const vec2 offset1 = offsets.zw * lightRadiusUV;

		visibilitySum += texture(shadowMap, vec3(uv + offset0, currentDepth));
		visibilitySum += texture(shadowMap, vec3(uv + offset1, currentDepth));

		samples += 2;
	}

	float visibility = visibilitySum / samples;

	if (visibility > 0.0 && visibility < 1.0)
	{
		const int totalSamples = EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE;
		for (int i = earlySamples / 2; i < totalSamples / 2; ++i)
		{
			const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, pixel), 0);
			const vec2 offset0 = offsets.xy * lightRadiusUV;
			const vec2 offset1 = offsets.zw * lightRadiusUV;

			visibilitySum += texture(shadowMap, vec3(uv + offset0, currentDepth));
			visibilitySum += texture(shadowMap, vec3(uv + offset1, currentDepth));

			samples += 2;
		}

		visibilitySum /= float(samples);
	}

	return visibility;
}

#endif

float DirLight_ShadowCalculation_Hard(sampler2DShadow shadowMap, vec3 fragPosLightSpace, float lightRadiusUV, int cascade)
{
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	const float currentDepth = fragPosLightSpace.z + s_BaseBias;

	return ShadowVisibility_PCF(shadowMap, uv, currentDepth);
}

float PointLight_ShadowCalculation_Hard(samplerCubeShadow depthTexture, vec3 samplePos, float NdotL, float farDistance)
{
	const int samples = 20;
	const float invSamples = 1.f / float(samples);
	const vec3 sampleOffsetDirections[samples] = vec3[]
	(
		vec3(1, 1, +1), vec3(+1, -1, +1), vec3(-1, -1, +1), vec3(-1, +1, +1),
		vec3(1, 1, -1), vec3(+1, -1, -1), vec3(-1, -1, -1), vec3(-1, +1, -1),
		vec3(1, 1, +0), vec3(+1, -1, +0), vec3(-1, -1, +0), vec3(-1, +1, +0),
		vec3(1, 0, +1), vec3(-1, +0, +1), vec3(+1, +0, -1), vec3(-1, +0, -1),
		vec3(0, 1, +1), vec3(+0, -1, +1), vec3(+0, -1, -1), vec3(+0, +1, -1)
	);

	const float bias = s_BaseBias;
	const float currentDepth = VectorToDepth(samplePos, farDistance, EG_POINT_LIGHT_NEAR) + bias;
	float visibility = 0.f;
	
	const float baseDiskRadius = 0.0001f;
	const float diskRadius = max(10.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);
	for (int i = 0; i < samples; ++i)
	{
		visibility += texture(depthTexture, vec4(samplePos + normalize(sampleOffsetDirections[i]) * diskRadius, currentDepth));
	}
	visibility *= invSamples;
	
	return visibility;
}

float SpotLight_ShadowCalculation_Hard(sampler2DShadow shadowMap, vec3 fragPosLightSpace, float lightRadiusUV)
{
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	const float currentDepth = fragPosLightSpace.z + s_BaseBias;

	return ShadowVisibility_PCF(shadowMap, uv, currentDepth);
}

vec3 DirLight_ColoredShadowCalculation_Hard(sampler2D coloredTexture, vec3 fragPosLightSpace)
{
	const vec2 uv = fragPosLightSpace.xy * 0.5f + 0.5f;
	return texture(coloredTexture, uv).rgb;
}

vec3 PointLight_ColoredShadowCalculation_Hard(samplerCube depthTexture, vec3 lightToFrag)
{
	return texture(depthTexture, lightToFrag).rgb;
}

vec3 SpotLight_ColoredShadowCalculation_Hard(sampler2D coloredTexture, vec3 fragPosLightSpace)
{
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	return texture(coloredTexture, uv).rgb;
}

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, lightRadiusUV, cascade) DirLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, lightRadiusUV, cascade)
#else
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, lightRadiusUV, cascade) DirLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, lightRadiusUV, cascade)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, NdotL, farDistance) PointLight_ShadowCalculation_Soft(depthTexture, lightToFrag, NdotL, farDistance)
#else
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, NdotL, farDistance) PointLight_ShadowCalculation_Hard(depthTexture, lightToFrag, NdotL, farDistance)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, lightRadiusUV) SpotLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, lightRadiusUV)
#else
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, lightRadiusUV) SpotLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, lightRadiusUV)
#endif

#ifdef EG_SOFT_SHADOWS
#define DirLight_ColoredShadowCalculation(depthTexture, fragPos_LS) DirLight_ColoredShadowCalculation_Hard(depthTexture, fragPos_LS)
#else
#define DirLight_ColoredShadowCalculation(depthTexture, fragPos_LS) DirLight_ColoredShadowCalculation_Hard(depthTexture, fragPos_LS)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define PointLight_ColoredShadowCalculation(depthTexture, lightToFrag) PointLight_ColoredShadowCalculation_Hard(depthTexture, lightToFrag)
#else
#define PointLight_ColoredShadowCalculation(depthTexture, lightToFrag) PointLight_ColoredShadowCalculation_Hard(depthTexture, lightToFrag)
#endif

#ifdef EG_SOFT_SHADOWS
#define SpotLight_ColoredShadowCalculation(depthTexture, fragPos_LS) SpotLight_ColoredShadowCalculation_Hard(depthTexture, fragPos_LS)
#else
#define SpotLight_ColoredShadowCalculation(depthTexture, fragPos_LS) SpotLight_ColoredShadowCalculation_Hard(depthTexture, fragPos_LS)
#endif

#endif
