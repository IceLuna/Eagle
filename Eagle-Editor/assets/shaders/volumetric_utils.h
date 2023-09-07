#ifndef EG_VOLUMETRIC_UTILS
#define EG_VOLUMETRIC_UTILS

#include "defines.h"

// Used resources for volumetric:
// https://www.alexandre-pestana.com/volumetric-lights/
// https://andrew-pham.blog/2019/10/03/volumetric-lighting/
// https://github.com/aabbtree77/twinpeekz
// https://github.com/metzzo/ezg17-transition/

float tri(float x)
{
	return abs(fract(x) - .5);
}

vec3 tri3(vec3 p)
{
	return vec3(
		tri(p.z + tri(p.y * 1.)),
		tri(p.z + tri(p.x * 1.)),
		tri(p.y + tri(p.x * 1.))
	);
}

// Taken from https://www.shadertoy.com/view/4ts3z2
// By NIMITZ  (twitter: @stormoid)
float Noise3D(vec3 p, float spd, float time)
{
	float z = 1.4;
	float rz = 0.;
	vec3 bp = p;
	for (float i = 0.; i <= 3.; i++)
	{
		vec3 dg = tri3(bp * 2.);
		p += (dg + time * spd);

		bp *= 1.8;
		z *= 1.5;
		p *= 1.2;
		//p.xz*= m2;

		rz += (tri(p.z + tri(p.x + tri(p.y)))) / z;
		bp += 0.14;
	}
	return rz;
}

float SampleFog(vec3 pos)
{
	return (Noise3D(pos * 2.2 / 8, 0.2f, g_Time) * 0.75) * 5.f; // 5 is an arbitrary number so that overall intensity doesn't drop
}

float PhaseFunction(float cosAngle)
{
	const float anisotropy = 0.f;
	const float nom = 1.f - anisotropy * anisotropy;
	const float denom = 4.f * EG_PI * pow(1 + anisotropy * anisotropy - 2 * anisotropy * cosAngle, 1.5f);
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
	const float baseBias = texelSize * (cascade == 0 ? 0.25f : 0.5f);
	float k = 0.f;
	switch (cascade)
	{
		case 1: k = 0.00009f; break;
		case 2: k = 0.0005f; break;
		case 3: k = 0.002f; break;
	}
	const float bias = max(baseBias * (1.0 - NdotL), baseBias) + k;

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
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float k = mix(30.f, 150.f, 1.f - NdotL);
	const float bias = texelSize * k;
	const vec3 normalBias = geometryNormal * bias;
	lightToFrag += normalBias;

	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);
	float shadow = 0.f;

	float closestDepth = texture(depthTexture, lightToFrag).r;
	if (currentDepth > closestDepth)
		shadow += 1.f;

	return 1.f - shadow;
}

float SpotLight_ShadowCalculation_Volumetric(sampler2D depthTexture, vec3 fragPosLightSpace, float NdotL)
{
	const vec2 texelSize = vec2(1.f) / vec2(textureSize(depthTexture, 0));
	const float baseBias = texelSize.x * 0.0007f;
	const float bias = max(5.f * baseBias * (1.f - NdotL), baseBias);
	const vec2 uv = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z + bias;

	float shadow = 0.f;
	const float closestDepth = texture(depthTexture, uv).r;
	if (currentDepth > closestDepth)
		shadow += 1.f;

	return 1.f - shadow;
}

vec3 DirLight_ColoredShadowCalculation_Volumetric(sampler2D depthTexture, sampler2D coloredDepthTexture, vec3 fragPosLightSpace, float NdotL, int cascade)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float baseBias = texelSize * 0.1f;
	float k = 0.f;
	switch (cascade)
	{
		case 1: k = 0.00009f; break;
		case 2: k = 0.0005f; break;
		case 3: k = 0.002f; break;
	}
	const float bias = max(baseBias * (1.0 - NdotL), baseBias) + k;
	const vec2 projCoords = (fragPosLightSpace.xy * 0.5f + 0.5f) + vec2(bias);

	const float currentDepth = fragPosLightSpace.z - bias;
	const float depth = texture(coloredDepthTexture, projCoords).r;
	if (currentDepth < depth)
		return vec3(1);

	return texture(depthTexture, projCoords).rgb;
}

vec3 PointLight_ColoredShadowCalculation_Volumetric(samplerCube depthTexture, samplerCube coloredDepthTexture, vec3 lightToFrag, vec3 geometryNormal, float NdotL)
{
	const float texelSize = 1.f / textureSize(depthTexture, 0).x;
	const float bias = texelSize * (1.f - NdotL) * 4.f;
	lightToFrag += bias;

	const float currentDepth = VectorToDepth(lightToFrag, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);
	const float depth = texture(coloredDepthTexture, lightToFrag).r;
	if (currentDepth < depth)
		return vec3(1);

	return texture(depthTexture, lightToFrag).rgb;
}

vec3 SpotLight_ColoredShadowCalculation_Volumetric(sampler2D coloredTexture, sampler2D coloredDepthTexture, vec3 fragPosLightSpace, float NdotL, float dist2)
{
	const float texelSize = 1.f / float(textureSize(coloredTexture, 0));
	const float baseBias = texelSize;
	const float bias = max(1.15f * baseBias * (1.f - NdotL), baseBias);
	const vec2 projCoords = (fragPosLightSpace * 0.5f + 0.5f).xy + vec2(bias);

	const float currentDepth = fragPosLightSpace.z;
	const float depth = texture(coloredDepthTexture, projCoords).r;
	if (currentDepth < depth)
		return vec3(1);

	return texture(coloredTexture, projCoords).rgb;
}

vec3 DirectionalLight_Volumetric(DirectionalLight light, sampler2D depthTextures[EG_CASCADES_COUNT],
#ifdef EG_TRANSLUCENT_SHADOWS
	sampler2D coloredTextures[EG_CASCADES_COUNT], sampler2D coloredDepthTextures[EG_CASCADES_COUNT],
#endif
	vec3 worldPos, vec3 cameraPos,
	vec3 incoming, vec3 normal, mat4 cameraView, uint scatteringSamples, float scatteringZFar, float maxShadowDistance)
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
#ifdef EG_TRANSLUCENT_SHADOWS
	vec3 result = vec3(0.0);
#else
	float result = 0.f;
#endif

	bool bCastsShadows = light.bCastsShadows != 0;
	for (uint i = 0; i < scatteringSamples; ++i)
	{
#ifdef EG_TRANSLUCENT_SHADOWS
		vec3 coloredVisibility = vec3(1.f);
#endif
		float visibility = bCastsShadows ? 0.f : 1.f;
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
#ifdef EG_TRANSLUCENT_SHADOWS
					coloredVisibility = DirLight_ColoredShadowCalculation_Volumetric(coloredTextures[nonuniformEXT(layer)], coloredDepthTextures[nonuniformEXT(layer)], lightSpacePos, NdotL, layer);
#endif
				}
			}
		}

#ifdef EG_VOLUMETRIC_FOG
		float fog = SampleFog(currentPos);
#else
		float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
		result += fog * coloredVisibility * visibility * PhaseFunction(dot(incoming, fragToCamNorm));
#else
		result += fog * visibility * PhaseFunction(dot(incoming, fragToCamNorm));
#endif
		
		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * light.VolumetricFogIntensity;
}

vec3 PointLight_Volumetric(in PointLight light, samplerCube shadowMap,
#ifdef EG_TRANSLUCENT_SHADOWS
	samplerCube coloredTexture, samplerCube coloredDepthTexture,
#endif
	vec3 worldPos, vec3 cameraPos,
	float NdotL, vec3 normal, uint scatteringSamples, float scatteringZFar, float maxShadowRange2, bool bCastsShadow)
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

#ifdef EG_TRANSLUCENT_SHADOWS
	vec3 result = vec3(0.0);
#else
	float result = 0.f;
#endif

	for (uint i = 0; i < scatteringSamples; ++i)
	{
		const vec3 incoming = light.Position - currentPos;
		const float distance2 = dot(incoming, incoming);
		if (distance2 < abs(light.Radius2))
		{
#ifdef EG_TRANSLUCENT_SHADOWS
			vec3 coloredVisibility = vec3(1.f);
#endif
			float visibility = bCastsShadow ? 0.f : 1.f;
			const float attenuation = 1.f / distance2;
			
			if (bCastsShadow && NOT_ZERO(attenuation))
			{
				if (distance2 < maxShadowRange2)
				{
					visibility = PointLight_ShadowCalculation_Volumetric(shadowMap, -incoming, normal, NdotL);
#ifdef EG_TRANSLUCENT_SHADOWS
					coloredVisibility = PointLight_ColoredShadowCalculation_Volumetric(coloredTexture, coloredDepthTexture , -incoming, normal, NdotL);
#endif
				}
			}
#ifdef EG_VOLUMETRIC_FOG
			float fog = SampleFog(currentPos);
#else
			float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
			result += fog * coloredVisibility * visibility * attenuation * PhaseFunction(dot(normalize(incoming), fragToCamNorm));
#else
			result += fog * visibility * attenuation * PhaseFunction(dot(normalize(incoming), fragToCamNorm));
#endif
		}

		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * abs(light.VolumetricFogIntensity);
}

vec3 SpotLight_Volumetric(in SpotLight light, sampler2D shadowMap,
#ifdef EG_TRANSLUCENT_SHADOWS
	sampler2D coloredTexture, sampler2D coloredDepthTexture,
#endif
	vec3 worldPos, vec3 cameraPos,
	float NdotL, vec3 normal, uint scatteringSamples, float scatteringZFar, float maxShadowRange2, bool bCastsShadow)
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

#ifdef EG_TRANSLUCENT_SHADOWS
	vec3 result = vec3(0.0);
#else
	float result = 0.f;
#endif
	const float texelSize = 1.f / textureSize(shadowMap, 0).x;

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
			float attenuation = 1.f / distance2;
			const vec3 normIncoming = normalize(incoming);
			const float theta = clamp(dot(normIncoming, normSpotDir), EG_FLT_SMALL, 1.0);
			const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);
			attenuation *= cutoffIntensity;

			float visibility = bCastsShadow ? 0.f : 1.f;
#ifdef EG_TRANSLUCENT_SHADOWS
			vec3 coloredShadow = vec3(1.f);
#endif
			if (bCastsShadow && NOT_ZERO(attenuation))
			{
				if (distance2 < maxShadowRange2)
				{
					const float k = 20.f + (40.f * light.OuterCutOffRadians * light.OuterCutOffRadians) + distance2 * 2.2f; // Some magic number that helps to fight against self-shadowing
					const float bias = texelSize * k;
					const vec3 normalBias = normIncoming * bias;
					vec4 lightSpacePos = light.ViewProj * vec4(currentPos + normalBias, 1.0);
					lightSpacePos.xyz /= lightSpacePos.w;

					visibility = SpotLight_ShadowCalculation_Volumetric(shadowMap, lightSpacePos.xyz, NdotL);
#ifdef EG_TRANSLUCENT_SHADOWS
					coloredShadow = SpotLight_ColoredShadowCalculation_Volumetric(coloredTexture, coloredDepthTexture, lightSpacePos.xyz, NdotL, distance2);
#endif
				}
			}
#ifdef EG_VOLUMETRIC_FOG
			float fog = SampleFog(currentPos);
#else
			float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
			result += fog * coloredShadow * visibility * attenuation * PhaseFunction(dot(normIncoming, fragToCamNorm));
#else
			result += fog * visibility * attenuation * PhaseFunction(dot(normIncoming, fragToCamNorm));
#endif
		}

		currentPos += deltaStep;
	}

	return result / scatteringSamples * light.LightColor * light.VolumetricFogIntensity;
}

#endif
