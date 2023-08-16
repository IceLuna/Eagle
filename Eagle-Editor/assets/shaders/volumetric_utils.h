#ifndef EG_VOLUMETRIC_UTILS
#define EG_VOLUMETRIC_UTILS

#include "defines.h"

// Used resources for volumetric:
// https://www.alexandre-pestana.com/volumetric-lights/
// https://andrew-pham.blog/2019/10/03/volumetric-lighting/
// https://github.com/aabbtree77/twinpeekz

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

float DirLight_ShadowCalculation_Volumetric(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL, int cascade)
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

	const vec2 uv = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z - bias;

	float shadow = 0.f;
	const float closestDepth = texture(depthTexture, uv).r;
	if (currentDepth > closestDepth)
		shadow += 1.f;

	return 1.f - shadow;
}

float PointLight_ShadowCalculation_Volumetric(samplerCube depthTexture, vec3 lightToFrag, vec3 geometryNormal, float NdotL)
{
	const int samples = 1;
	const float invSamples = 1.f / float(samples);
	const vec3 sampleOffsetDirections[samples] = vec3[]
	(
		vec3(1, 1, +1)
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

	return 1.f - shadow;
}

float SpotLight_ShadowCalculation_Volumetric(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.0007f;
	const float bias = max(10.f * baseBias * (1.f - NdotL), baseBias);
	const vec2 uv = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z + bias;

	float shadow = 0.f;
	const float closestDepth = texture(depthTexture, uv).r;
	if (currentDepth > closestDepth)
		shadow += 1.f;

	return 1.f - shadow;
}

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
				int layer = -1;
				for (int i = 0; i < EG_CASCADES_COUNT; ++i)
				{
					if (cascadeDepth < light.CascadePlaneDistances[i])
					{
						layer = i;
						break;
					}
				}
				if (layer != -1)
				{
					const float texelSize = 1.f / textureSize(depthTextures[nonuniformEXT(layer)], 0).x;
					const float bias = texelSize * k;
					const vec3 normalBias = normal * bias;

					const vec3 lightSpacePos = (light.ViewProj[layer] * vec4(currentPos + normalBias, 1.0)).xyz;
					visibility = DirLight_ShadowCalculation_Volumetric(depthTextures[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
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
					visibility = PointLight_ShadowCalculation_Volumetric(shadowMap, -incoming, normal, NdotL);
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

					visibility = SpotLight_ShadowCalculation_Volumetric(shadowMap, lightSpacePos.xyz, NdotL);
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
