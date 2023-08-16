#ifndef EG_SHADOWS_UTILS
#define EG_SHADOWS_UTILS

#include "defines.h"

float VectorToDepth(vec3 val, float n, float f)
{
	const vec3 absVec = abs(val);
	const float localZcomp = max(absVec.x, max(absVec.y, absVec.z));

	const float normZComp = (f + n) / (f - n) - (2.f * f * n) / (f - n) / localZcomp;
	return normZComp * 0.5f + 0.5f;
}

int GetCascadeIndex(in DirectionalLight light, float cascadeDepth)
{
	for (int i = 0; i < EG_CASCADES_COUNT; ++i)
	{
		if (cascadeDepth < light.CascadePlaneDistances[i])
			return i;
	}
	return -1;
}

#ifdef EG_SOFT_SHADOWS
float DirLight_ShadowCalculation_Soft(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL, int cascade)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float baseBias = texelSize * 0.25f; // use texelSize?
	float k = 0.f;
	switch (cascade)
	{
		case 1: k = 0.00009f; break;
		case 2: k = 0.0005f; break;
		case 3: k = 0.002f; break;
	}
	const float bias = -max(baseBias * (1.0 - NdotL), baseBias) + k;

	const float currentDepth = fragPosLightSpace.z - bias;
	const vec2 shadowCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const ivec2 f = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	const float invSamplesCount = 1.f / 8.f;
	float sum = 0.f;
	for (int i = 0; i < 4; ++i)
	{
		const vec3 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0).rgb * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
		vec2 uv = shadowCoords + offsets.rg * texelSize;
		float closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;

		uv = shadowCoords + offsets.br * texelSize;
		closestDepth = texture(depthTexture, uv).x;
		if (currentDepth > closestDepth)
			sum += 1.f;
	}
	float shadow = sum * invSamplesCount;

	if (NOT_ZERO(shadow) && NOT_ONE(shadow))
	{
		const int samplesDiv2 = int(float(EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE) * 0.5f);

		for (int i = 4; i < samplesDiv2; ++i)
		{
			const vec3 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0).rgb * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
			vec2 uv = shadowCoords + offsets.rg * texelSize;
			float closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;

			uv = shadowCoords + offsets.br * texelSize;
			closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;
		}

		shadow = sum / float(samplesDiv2 * 2.f);
	}

	return (1.f - shadow);
}

float PointLight_ShadowCalculation_Soft(samplerCube depthTexture, vec3 lightToFrag, vec3 geometryNormal, float NdotL)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float k = 20.f;
	const float bias = texelSize * k;
	const vec3 normalBias = geometryNormal * bias;
	lightToFrag += normalBias;

	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);

	const ivec2 f = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	const int samples = 8;
	const float invSamples = 1.f / float(samples);
	float sum = 0.f;

	const float baseDiskRadius = 0.0025f;
	const float diskRadius = max(2.f * baseDiskRadius * (1.f - NdotL), baseDiskRadius);
	for (int i = 0; i < 4; ++i)
	{
		const vec3 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0).rgb * EG_SM_DISTRIBUTION_RANDOM_RADIUS;

		vec3 uv = lightToFrag + offsets.rgb * diskRadius;
		float closestDepth = texture(depthTexture, uv).r;
		if (currentDepth > closestDepth)
			sum += 1.f;

		uv = lightToFrag + offsets.brg * diskRadius;
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
			const vec3 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0).rgb * EG_SM_DISTRIBUTION_RANDOM_RADIUS;

			vec3 uv = lightToFrag + offsets.rgb * diskRadius;
			float closestDepth = texture(depthTexture, uv).r;
			if (currentDepth > closestDepth)
				sum += 1.f;

			uv = lightToFrag + offsets.brg * diskRadius;
			closestDepth = texture(depthTexture, uv).x;
			if (currentDepth > closestDepth)
				sum += 1.f;
		}

		shadow = sum / float(samplesDiv2 * 2.f);
	}

	return (1.f - shadow);
}

float SpotLight_ShadowCalculation_Soft(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.002f;
	const float bias = max(10.f * baseBias * (1.f - NdotL), baseBias);

	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 shadowCoords = fragPosLightSpace.xy * 0.5f + 0.5f;
	const ivec2 f = ivec2(mod(EG_PIXEL_COORDS, vec2(EG_SM_DISTRIBUTION_TEXTURE_SIZE)));

	const float invSamplesCount = 1.f / 8.f;
	float sum = 0.f;
	for (int i = 0; i < 4; ++i)
	{
		const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
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
		const int samplesDiv2 = int(float(EG_SM_DISTRIBUTION_FILTER_SIZE * EG_SM_DISTRIBUTION_FILTER_SIZE) * 0.5f);

		for (int i = 4; i < samplesDiv2; ++i)
		{
			const vec4 offsets = texelFetch(g_SmDistribution, ivec3(i, f), 0) * EG_SM_DISTRIBUTION_RANDOM_RADIUS;
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

#endif

float DirLight_ShadowCalculation_Hard(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL, int cascade)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float baseBias = texelSize * 0.25f; // use texelSize?
	float k = 0.f;
	switch (cascade)
	{
		case 1: k = 0.00009f; break;
		case 2: k = 0.0005f; break;
		case 3: k = 0.002f; break;
	}
	const float bias = -max(baseBias * (1.0 - NdotL), baseBias) + k;
	
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

float PointLight_ShadowCalculation_Hard(samplerCube depthTexture, vec3 lightToFrag, vec3 geometryNormal, float NdotL)
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

	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float k = 20.f;
	const float bias = texelSize * k;
	const vec3 normalBias = geometryNormal * bias;
	lightToFrag += normalBias;

	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);
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

float SpotLight_ShadowCalculation_Hard(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.0007f;
	const float bias = max(10.f * baseBias * (1.f - NdotL), baseBias);
	const vec2 projCoords = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z + bias;

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
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL, cascade) DirLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, NdotL, cascade)
#else
#define DirLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL, cascade) DirLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, NdotL, cascade)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, geometryNormal, NdotL) PointLight_ShadowCalculation_Soft(depthTexture, lightToFrag, geometryNormal, NdotL)
#else
#define PointLight_ShadowCalculation(depthTexture, lightToFrag, geometryNormal, NdotL) PointLight_ShadowCalculation_Hard(depthTexture, lightToFrag, geometryNormal, NdotL)
#endif

// 0 = in shadow, 1 = not in shadow
#ifdef EG_SOFT_SHADOWS
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) SpotLight_ShadowCalculation_Soft(depthTexture, fragPos_LS, NdotL)
#else
#define SpotLight_ShadowCalculation(depthTexture, fragPos_LS, NdotL) SpotLight_ShadowCalculation_Hard(depthTexture, fragPos_LS, NdotL)
#endif

#ifdef EG_VOLUMETRIC_LIGHT

// Used resources for volumetric:
// https://github.com/aabbtree77/twinpeekz, which is based on -> https://www.alexandre-pestana.com/volumetric-lights/

float PhaseFunction(vec3 inDir, vec3 outDir)
{
	float anisotropy = 0.0f;
	float cosAngle = dot(inDir, outDir) / (length(inDir) * length(outDir));
	float nom = 1.f - anisotropy * anisotropy;
	float denom = 4.f * EG_PI * pow(1 + anisotropy * anisotropy - 2 * anisotropy * cosAngle, 1.5f);
	return nom / denom;
}

const mat4 DITHER_PATTERN = mat4(
	vec4(0.0f, 0.5f, 0.125f, 0.625f),
	vec4(0.75f, 0.22f, 0.875f, 0.375f),
	vec4(0.1875f, 0.6875f, 0.0625f, 0.5625f),
	vec4(0.9375f, 0.4375f, 0.8125f, 0.3125f)
);

vec3 DirectionalLight_Volumetric(DirectionalLight light, sampler2D depthTextures[EG_CASCADES_COUNT], vec3 worldPos, vec3 cameraPos, vec3 incoming, vec3 normal, mat4 cameraView, uint scatteringSamples, float scatteringZFar, float maxShadowDistance)
{
	const bool bVolumetricLight = light.bVolumetricLight != 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	vec3 camToFrag = worldPos - cameraPos;
	float camToFragLen = length(camToFrag);
	const vec3 camToFragNorm = camToFrag / camToFragLen;
	if (camToFragLen > scatteringZFar)
	{
		camToFrag = camToFragNorm * scatteringZFar;
		camToFragLen = scatteringZFar;
	}
	const vec3 deltaStep = camToFrag / (scatteringSamples + 1);
	const vec3 fragToCamNorm = -camToFragNorm;
	vec3 currentPos = cameraPos;

	const float rand = DITHER_PATTERN[int(EG_PIXEL_COORDS.x) % 4][int(EG_PIXEL_COORDS.y) % 4];
	currentPos += deltaStep * rand;

	const float NdotL = clamp(dot(incoming, normal), EG_FLT_SMALL, 1.0);
	const float k = 100.f;
	float result = 0.0;

	bool bCastsShadows = light.bCastsShadows != 0;
	for (uint i = 0; i < scatteringSamples; ++i)
	{
		float visibility = 1.f;
		if (bCastsShadows)
		{
			if (length(currentPos - cameraPos) < maxShadowDistance)
			{
				const float cascadeDepth = abs((cameraView * vec4(currentPos, 1.0)).z);
				const int layer = GetCascadeIndex(light, cascadeDepth);
				if (layer != -1)
				{
					const float texelSize = 1.f / textureSize(depthTextures[nonuniformEXT(layer)], 0).x;
					const float bias = texelSize * k;
					const vec3 normalBias = normal * bias;

					const vec3 lightSpacePos = (light.ViewProj[layer] * vec4(currentPos + normalBias, 1.0)).xyz;
					visibility = DirLight_ShadowCalculation(depthTextures[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
				}
			}
		}

		result += visibility * PhaseFunction(incoming, fragToCamNorm);
		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * light.VolumetricFogIntensity;
}

vec3 PointLight_Volumetric(in PointLight light, samplerCube shadowMap, vec3 worldPos, vec3 cameraPos, float NdotL, vec3 normal, uint scatteringSamples, float scatteringZFar, float maxShadowRange, bool bCastsShadow)
{
	const bool bVolumetricLight = (floatBitsToUint(light.VolumetricFogIntensity) & 0x80000000) != 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	vec3 camToFrag = worldPos - cameraPos;
	float camToFragLen = length(camToFrag);
	const vec3 camToFragNorm = camToFrag / camToFragLen;
	if (camToFragLen > scatteringZFar)
	{
		camToFrag = camToFragNorm * scatteringZFar;
		camToFragLen = scatteringZFar;
	}
	const vec3 deltaStep = camToFrag / (scatteringSamples + 1);
	const vec3 fragToCamNorm = -camToFragNorm;
	vec3 currentPos = cameraPos;

	const float rand = DITHER_PATTERN[int(EG_PIXEL_COORDS.x) % 4][int(EG_PIXEL_COORDS.y) % 4];
	currentPos += deltaStep * rand;

	float result = 0.0;
	const float maxShadowRange2 = maxShadowRange * maxShadowRange;

	for (uint i = 0; i < scatteringSamples; ++i)
	{
		const vec3 incoming = light.Position - currentPos;
		const float distance2 = dot(incoming, incoming);
		if (distance2 < abs(light.Radius2))
		{
			float visibility = bCastsShadow ? 0.f : 1.f;
			if (bCastsShadow)
			{
				if (distance2 < maxShadowRange2)
					visibility = PointLight_ShadowCalculation(shadowMap, -incoming, normal, NdotL);
			}
			const float attenuation = 1.f / distance2;
			result += attenuation * visibility * PhaseFunction(normalize(incoming), fragToCamNorm);
		}

		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * abs(light.VolumetricFogIntensity);
}

vec3 SpotLight_Volumetric(in SpotLight light, sampler2D shadowMap, vec3 worldPos, vec3 cameraPos, float NdotL, vec3 normal, uint scatteringSamples, float scatteringZFar, float maxShadowRange, bool bCastsShadow)
{
	const bool bVolumetricLight = light.bVolumetricLight != 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	vec3 camToFrag = worldPos - cameraPos;
	float camToFragLen = length(camToFrag);
	const vec3 camToFragNorm = camToFrag / camToFragLen;
	if (camToFragLen > scatteringZFar)
	{
		camToFrag = camToFragNorm * scatteringZFar;
		camToFragLen = scatteringZFar;
	}
	const vec3 deltaStep = camToFrag / (scatteringSamples + 1);
	const vec3 fragToCamNorm = -camToFragNorm;
	vec3 currentPos = cameraPos;

	const float rand = DITHER_PATTERN[int(EG_PIXEL_COORDS.x) % 4][int(EG_PIXEL_COORDS.y) % 4];
	currentPos += deltaStep * rand;

	float result = 0.0;
	const float texelSize = 1.f / textureSize(shadowMap, 0).x;
	const float maxShadowRange2 = maxShadowRange * maxShadowRange;

	//Cutoff
	const float innerCutOffCos = cos(light.InnerCutOffRadians);
	const float outerCutOffCos = cos(light.OuterCutOffRadians);
	const float epsilon = innerCutOffCos - outerCutOffCos;
	const vec3 normSpotDir = normalize(-light.Direction);

	for (uint i = 0; i < scatteringSamples; ++i)
	{
		const vec3 incoming = light.Position - currentPos;
		const float distance2 = dot(incoming, incoming);
		if (distance2 < light.Distance2)
		{
			float visibility = bCastsShadow ? 0.f : 1.f;
			if (bCastsShadow)
			{
				if (distance2 < maxShadowRange2)
				{
					const float k = 20.f + (40.f * light.OuterCutOffRadians) + distance2 * 2.2f; // Some magic number that helps to fight against self-shadowing
					const float bias = texelSize * k;
					const vec3 normalBias = normal * bias;
					vec4 lightSpacePos = light.ViewProj * vec4(currentPos + normalBias, 1.0);
					lightSpacePos.xyz /= lightSpacePos.w;

					visibility = SpotLight_ShadowCalculation(shadowMap, lightSpacePos.xyz, NdotL);
				}
			}
			
			float attenuation = 1.f / distance2;
			const vec3 normIncoming = normalize(incoming);

			//Cutoff
			const float theta = clamp(dot(normIncoming, normSpotDir), EG_FLT_SMALL, 1.0);
			const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);
			attenuation *= cutoffIntensity;

			result += visibility * PhaseFunction(normalize(incoming), fragToCamNorm) * attenuation;
		}

		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * light.VolumetricFogIntensity;
}
#endif

#endif
