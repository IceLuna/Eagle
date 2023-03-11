#ifndef EG_SHADOWS_UTILS
#define EG_SHADOWS_UTILS

#include "defines.h"

float VectorToDepth(vec3 val, float n, float f)
{
	vec3 absVec = abs(val);
	float localZcomp = max(absVec.x, max(absVec.y, absVec.z));

	float normZComp = (f + n) / (f - n) - (2.f * f * n) / (f - n) / localZcomp;
	return (normZComp + 1.f) * 0.5f;
}

float DirLight_ShadowCalculation_Soft(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.0005f;
	const float bias = max(baseBias * (1.f - NdotL), baseBias);

	const int samplesDiv2 = int(float(EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE) * 0.5f);
	const float currentDepth = fragPosLightSpace.z - bias;
	const vec2 shadowCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const vec2 f = mod(gl_FragCoord.xy, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE));
	ivec3 offsetCoords = ivec3(0, ivec2(f));

	const float invSamplesCount = 1.f / 8.f;
	float sum = 0.f;
	for (int i = 0; i < 4; ++i)
	{
		offsetCoords.x = i;
		vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
		vec2 uv = shadowCoords + offsets.rg * texelSize;
		float closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;

		uv = shadowCoords + offsets.ba * texelSize;
		closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;
	}
	float shadow = sum * invSamplesCount;

	if (NOT_ZERO(shadow) && NOT_ONE(shadow))
	{
		for (int i = 4; i < samplesDiv2; ++i)
		{
			offsetCoords.x = i;
			vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
			vec2 uv = shadowCoords + offsets.rg * texelSize;
			float closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;

			uv = shadowCoords + offsets.ba * texelSize;
			closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;
		}

		shadow = sum / float(samplesDiv2 * 2.f);
	}

	return (1.f - shadow);
}

float DirLight_ShadowCalculation_Hard(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.0005f;
	const float bias = max(baseBias * (1.f - NdotL), baseBias);
	const vec2 projCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z - bias;

	const int pcfSize = 3;
	const int pcfRange = pcfSize / 2;
	const float invPCFMatrixSize = 1.f / (pcfSize * pcfSize);

	float shadow = 0.f;
	for (int x = -pcfRange; x <= pcfRange; ++x)
		for (int y = -pcfRange; y <= pcfRange; ++y)
		{
			const vec2 uv = projCoords + vec2(x, y) * texelSize;
			const float closestDepth = texture(depthTexture, uv).r;
			if (currentDepth > closestDepth)
				shadow += 1.f;
		}

	return (1.f - (shadow * invPCFMatrixSize));
}

float PointLight_ShadowCalculation_Soft(samplerCube depthTexture, vec3 lightToFrag, float NdotL)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float baseBias = texelSize * 0.0001f;
	const float bias = max(baseBias * (1.f - NdotL), baseBias);
	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR) - bias;
	
	const vec2 f = mod(gl_FragCoord.xy, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE));
	ivec3 offsetCoords = ivec3(0, ivec2(f));
	
	const int samples = 8;
	const float invSamples = 1.f / float(samples);
	float sum = 0.f;
	
	const float baseDiskRadius = 0.0025f;
	const float diskRadius = max(2.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);
	for (int i = 0; i < 4; ++i)
	{
		offsetCoords.x = i;
		vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
	
		vec3 uv = lightToFrag + offsets.rgb * diskRadius;
		float closestDepth = texture(depthTexture, uv).r;
		if (currentDepth > closestDepth)
			sum += 1.f;
	
		uv = lightToFrag + offsets.gba * diskRadius;
		closestDepth = texture(depthTexture, uv).r;
		if (currentDepth > closestDepth)
			sum += 1.f;
	}
	float shadow = sum * invSamples;
	
	if (NOT_ZERO(shadow) && NOT_ONE(shadow))
	{
		const int samplesDiv2 = int(float(EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE) * 0.5f);
	
		for (int i = 4; i < samplesDiv2; ++i)
		{
			offsetCoords.x = i;
			vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
	
			vec3 uv = lightToFrag + offsets.rgb * diskRadius;
			float closestDepth = texture(depthTexture, uv).r;
			if (currentDepth > closestDepth)
				sum += 1.f;
	
			uv = lightToFrag + offsets.gba * diskRadius;
			closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;
		}
	
		shadow = sum / float(samplesDiv2 * 2.f);
	}
	
	return (1.f - shadow);
}

float PointLight_ShadowCalculation_Hard(samplerCube depthTexture, vec3 lightToFrag, float NdotL)
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

	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.001f;
	const float bias = max(10.f * baseBias * (1.f - NdotL), baseBias);
	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR) - bias;
	float shadow = 0.f;

	const float baseDiskRadius = 0.001f;
	const float diskRadius = max(10.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);
	for (int i = 0; i < samples; ++i)
	{
		float closestDepth = texture(depthTexture, lightToFrag + sampleOffsetDirections[i] * diskRadius).r;
		if (currentDepth > closestDepth)
			shadow += 1.f;
	}
	shadow *= invSamples;

	return (1.f - shadow);
}

float SpotLight_ShadowCalculation_Soft(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.05f;
	const float bias = max(5.f * baseBias * (1.f - NdotL), baseBias);

	const int samplesDiv2 = int(float(EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE) * 0.5f);
	const float currentDepth = fragPosLightSpace.z - bias;
	const vec2 shadowCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const vec2 f = mod(gl_FragCoord.xy, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE));
	ivec3 offsetCoords = ivec3(0, ivec2(f));

	const float invSamplesCount = 1.f / 8.f;
	float sum = 0.f;
	for (int i = 0; i < 4; ++i)
	{
		offsetCoords.x = i;
		vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
		vec2 uv = shadowCoords + offsets.rg * texelSize;
		float closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;

		uv = shadowCoords + offsets.ba * texelSize;
		closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;
	}
	float shadow = sum * invSamplesCount;

	if (NOT_ZERO(shadow) && NOT_ONE(shadow))
	{
		for (int i = 4; i < samplesDiv2; ++i)
		{
			offsetCoords.x = i;
			vec4 offsets = texelFetch(g_SmDistribution, offsetCoords, 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
			vec2 uv = shadowCoords + offsets.rg * texelSize;
			float closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;

			uv = shadowCoords + offsets.ba * texelSize;
			closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;
		}

		shadow = sum / float(samplesDiv2 * 2.f);
	}

	return (1.f - shadow);
}

float SpotLight_ShadowCalculation_Hard(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.05f;
	const float bias = max(5.f * baseBias * (1.f - NdotL), baseBias);
	const vec2 projCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z - bias;

	const int pcfSize = 3;
	const int pcfRange = pcfSize / 2;
	const float invPCFMatrixSize = 1.f / (pcfSize * pcfSize);

	float shadow = 0.f;
	for (int x = -pcfRange; x <= pcfRange; ++x)
		for (int y = -pcfRange; y <= pcfRange; ++y)
		{
			const vec2 uv = projCoords + vec2(x, y) * texelSize;
			const float closestDepth = texture(depthTexture, uv).r;
			if (currentDepth > closestDepth)
				shadow += 1.f;
		}

	return (1.f - (shadow * invPCFMatrixSize));
}

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) DirLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, NdotL)
#else
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) DirLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, NdotL)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, NdotL) PointLight_ShadowCalculation_Soft(depthTexture, lightToFrag, NdotL)
#else
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, NdotL) PointLight_ShadowCalculation_Hard(depthTexture, lightToFrag, NdotL)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) SpotLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, NdotL)
#else
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) SpotLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, NdotL)
#endif

#endif
