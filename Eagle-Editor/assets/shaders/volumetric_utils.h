#ifndef EG_VOLUMETRIC_UTILS
#define EG_VOLUMETRIC_UTILS

#include "defines.h"
#include "shadow_maps/shadows_utils.h"

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

float PhaseFunction(float anisotropy, float cosAngle)
{
	const float anisotropy2 = anisotropy * anisotropy;
	const float nom = 1.f - anisotropy2;
	const float denom = 4.f * EG_PI * pow(1 + anisotropy2 - 2 * anisotropy * cosAngle, 1.5f);
	return nom / denom;
}

const float DITHER_PATTERN[16] = float[](
	0.0f, 0.5f, 0.125f, 0.625f,
	0.75f, 0.22f, 0.875f, 0.375f,
	0.1875f, 0.6875f, 0.0625f, 0.5625f,
	0.9375f, 0.4375f, 0.8125f, 0.3125f
);

float DirLight_ShadowCalculation_Volumetric(sampler2DShadow shadowMap, vec3 fragPosLightSpace)
{
	const float texelSize = 1.0 / float(textureSize(shadowMap, 0).x);
	const float bias = s_BaseBias;
	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 uv = fragPosLightSpace.xy * 0.5 + 0.5;
	
	return texture(shadowMap, vec3(uv, currentDepth));
}

float PointLight_ShadowCalculation_Volumetric(samplerCubeShadow depthTexture, vec3 samplePos, float farDistance)
{
	const float bias = s_BaseBias;
	const float currentDepth = VectorToDepth(samplePos, farDistance, EG_POINT_LIGHT_NEAR) + bias;
	
	return texture(depthTexture, vec4(samplePos, currentDepth));
}

float SpotLight_ShadowCalculation_Volumetric(sampler2DShadow depthTexture, vec3 fragPosLightSpace)
{
	const float bias = s_BaseBias;
	const vec2 uv = (fragPosLightSpace * 0.5f + 0.5f).xy;
	const float currentDepth = fragPosLightSpace.z + bias;
	
	return texture(depthTexture, vec3(uv, currentDepth)).r;
}

vec3 DirLight_ColoredShadowCalculation_Volumetric(sampler2D depthTexture, sampler2D coloredDepthTexture, vec3 fragPosLightSpace)
{
	const float bias = s_BaseBias;
	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 projCoords = fragPosLightSpace.xy * 0.5f + 0.5f;
	
	const float depth = texture(coloredDepthTexture, projCoords).r;
	if (currentDepth > depth)
		return vec3(1);
	
	return texture(depthTexture, projCoords).rgb;
}

vec3 PointLight_ColoredShadowCalculation_Volumetric(samplerCube depthTexture, samplerCube coloredDepthTexture, vec3 samplePos, float farDistance)
{
	const float bias = s_BaseBias;
	const float currentDepth = VectorToDepth(samplePos, farDistance, EG_POINT_LIGHT_NEAR) + bias;
	const float depth = texture(coloredDepthTexture, samplePos).r;
	if (currentDepth > depth)
		return vec3(1);
	
	return texture(depthTexture, samplePos).rgb;
}

vec3 SpotLight_ColoredShadowCalculation_Volumetric(sampler2D coloredTexture, sampler2D coloredDepthTexture, vec3 fragPosLightSpace)
{
	const float texelSize = 1.0 / float(textureSize(coloredDepthTexture, 0).x);
	const float bias = s_BaseBias;
	const float currentDepth = fragPosLightSpace.z + bias;
	const vec2 projCoords = fragPosLightSpace.xy * 0.5 + 0.5;
	
	const float depth = texture(coloredDepthTexture, projCoords).r;
	if (currentDepth > depth)
		return vec3(1);
	
	return texture(coloredTexture, projCoords).rgb;
}

vec3 DirectionalLight_Volumetric(DirectionalLight light, sampler2DShadow depthTextures[EG_CASCADES_COUNT],
#ifdef EG_TRANSLUCENT_SHADOWS
	sampler2D coloredTextures[EG_CASCADES_COUNT], sampler2D coloredDepthTextures[EG_CASCADES_COUNT],
#endif
	vec3 worldPos, vec3 cameraPos,
	vec3 incoming, vec3 normal, mat4 cameraView, uint scatteringSamples, float scatteringZFar)
{
	const bool bVolumetricLight = light.VolumetricFogIntensity < 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	vec3 camToFrag = worldPos - cameraPos;
	float camToFragLen = length(camToFrag);
	const vec3 camDir = camToFrag / camToFragLen;
	if (camToFragLen > scatteringZFar)
	{
		camToFrag = camDir * scatteringZFar;
		camToFragLen = scatteringZFar;
	}

	const float deltaStep = (camToFragLen) / float(scatteringSamples);
	float currentT = deltaStep * 0.5f;
	float tempT = currentT;
	
#ifdef EG_TRANSLUCENT_SHADOWS
	vec3 result = vec3(0.0);
#else
	float result = 0.f;
#endif

	bool bCastsShadows = (light.Flags & EG_DIR_LIGHT_CASTS_SHADOWS_MASK) != 0;
	for (uint i = 0; i < scatteringSamples && (currentT < camToFragLen); ++i)
	{
		vec3 currentPos = cameraPos + camDir * currentT;

#ifdef EG_TRANSLUCENT_SHADOWS
		vec3 coloredVisibility = vec3(1.f);
#endif
		float visibility = bCastsShadows ? 0.f : 1.f;
		if (bCastsShadows)
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
				const mat4 viewProj = g_LightMatrices[light.ViewProjOffset + layer];
				const vec3 lightSpacePos = GetDirectionalLightSamplePosition(depthTextures[nonuniformEXT(layer)], viewProj, currentPos, normal, incoming, layer);
				visibility = DirLight_ShadowCalculation_Volumetric(depthTextures[nonuniformEXT(layer)], lightSpacePos);
#ifdef EG_TRANSLUCENT_SHADOWS
				coloredVisibility = DirLight_ColoredShadowCalculation_Volumetric(coloredTextures[nonuniformEXT(layer)], coloredDepthTextures[nonuniformEXT(layer)], lightSpacePos);
#endif
			}
		}

#ifdef EG_VOLUMETRIC_FOG
		float fog = SampleFog(currentPos);
#else
		float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
		result += fog * coloredVisibility * visibility * PhaseFunction(g_FogAnisotropy, dot(incoming, camDir));
#else
		result += fog * visibility * PhaseFunction(g_FogAnisotropy, dot(incoming, camDir));
#endif

		tempT += deltaStep;

		const float rand = DITHER_PATTERN[i % 16];
		const float jitter = (rand - 0.5) * deltaStep;
		currentT = tempT + jitter;
	}

	return result / scatteringSamples * light.LightColor * abs(light.VolumetricFogIntensity);
}

bool SphereIntersect(vec3 ro, vec3 rd, vec3 sphere, float radius2, out float t0, out float t1)
{
	vec3 tmp = ro - sphere;

	float b = dot(rd, tmp);
	float c = dot(tmp, tmp) - radius2;

	float disc = b * b - c;

	if (disc < 0.0)
	{
		return false;
	}

	disc = sqrt(disc);

	t0 = -b - disc;
	t1 = -b + disc;

	return true;
}

vec3 PointLight_Volumetric(in PointLight light, samplerCubeShadow shadowMap,
#ifdef EG_TRANSLUCENT_SHADOWS
	samplerCube coloredTexture, samplerCube coloredDepthTexture,
#endif
	vec3 worldPos, vec3 cameraPos,
	vec3 normal, uint scatteringSamples, float scatteringZFar, bool bCastsShadow)
{
	const bool bVolumetricLight = light.VolumetricFogIntensity < 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	const vec3 camToFrag = worldPos - cameraPos;
	const float camToFragLen = length(camToFrag);
	const vec3 camDir = camToFrag / camToFragLen;
	const float radius2 = abs(light.Radius2);
	float t0, t1;

	if (!SphereIntersect(
		cameraPos,
		camDir,
		light.Position,
		radius2,
		t0, t1))
	{
		return vec3(0);
	}

	// Adjust sampling interval and origin
	t1 = min(t1, camToFragLen);
	t0 = clamp(t0, 0.0, t1);

	if (t0 == t1) // warn: can also be equal when t > radius!
	{
		return vec3(0);
	}

	if (t1 < g_Near || t0 > camToFragLen || t0 > scatteringZFar)
	{
		return vec3(0);
	}

	const float deltaStep = (t1 - t0) / float(scatteringSamples);
	float currentT = (t0 + deltaStep * 0.5f);
	float tempT = currentT;

#ifdef EG_TRANSLUCENT_SHADOWS
	vec3 result = vec3(0.0);
#else
	float result = 0.f;
#endif

	for (uint i = 0; i < scatteringSamples && (currentT < camToFragLen); ++i)
	{
		vec3 currentPos = cameraPos + camDir * currentT;

		const vec3 incoming = light.Position - currentPos;
		const float distance2 = dot(incoming, incoming);
		const float attenuation = 1.f / distance2
			* EG_SQUARE(clamp(1.0 - EG_SQUARE(distance2 * 1.0f / radius2), 0.f, 1.f));
		if (distance2 < radius2 && NOT_ZERO(attenuation))
		{
#ifdef EG_TRANSLUCENT_SHADOWS
			vec3 coloredVisibility = vec3(1.f);
#endif
			float visibility = bCastsShadow ? 0.f : 1.f;
			
			if (bCastsShadow)
			{
				const float NdotL = clamp(dot(normalize(incoming), normal), 0, 1.0);
				const vec3 samplePos = GetPointLightSamplePosition(shadowMap, light, currentPos, normal, NdotL, sqrt(distance2));
				visibility = PointLight_ShadowCalculation_Volumetric(shadowMap, samplePos, light.Radius);
#ifdef EG_TRANSLUCENT_SHADOWS
				coloredVisibility = PointLight_ColoredShadowCalculation_Volumetric(coloredTexture, coloredDepthTexture, samplePos, light.Radius);
#endif
			}
#ifdef EG_VOLUMETRIC_FOG
			float fog = SampleFog(currentPos);
#else
			float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
			result += fog * coloredVisibility * visibility * attenuation * PhaseFunction(g_FogAnisotropy, dot(normalize(incoming), camDir));
#else
			result += fog * visibility * attenuation * PhaseFunction(g_FogAnisotropy, dot(normalize(incoming), camDir));
#endif
		}

		tempT += deltaStep;

		const float rand = DITHER_PATTERN[i % 16];
		const float jitter = (rand - 0.5) * deltaStep;
		currentT = tempT + jitter;
	}

	return result / scatteringSamples * light.LightColor * abs(light.VolumetricFogIntensity);
}

// Cone-ray intersection routine
// Based on: http://lousodrome.net/blog/light/2017/01/03/intersection-of-a-ray-and-a-cone/
bool ConeIntersect(vec3 ro, vec3 rd, vec3 conePoint, vec3 axis, float h, float cosTheta, out float t0, out float t1)
{
	t0 = t1 = 0.0;

	// looking for intersect ray and infinity cone
	vec3 co = ro - conePoint;

	float rdAxis = dot(rd, axis);
	float coAxis = dot(co, axis);

	float a = rdAxis * rdAxis - cosTheta * cosTheta;
	float b = rdAxis * coAxis - dot(rd, co) * cosTheta * cosTheta;
	float c = coAxis * coAxis - dot(co, co) * cosTheta * cosTheta;

	bool bT0Valid, bT1Valid;

	if (a == 0)
	{
		t0 = -c / b;
		bT0Valid = true;
		bT1Valid = false;
	}
	else
	{
		float det = b * b - a * c;
		if (det < 0.0)
		{
			return false;
		}

		det = sqrt(det);
		float sol0 = (-b - det) / a;
		float sol1 = (-b + det) / a;
		t0 = min(sol0, sol1);
		t1 = max(sol0, sol1);

		// check if points are in limited cone
		float h0 = dot(ro + t0 * rd - conePoint, axis);
		float h1 = dot(ro + t1 * rd - conePoint, axis);

		bT0Valid = (h0 >= 0.0) && (h0 <= h);
		bT1Valid = (h1 >= 0.0) && (h1 <= h);
	}

	if (bT0Valid && bT1Valid)
	{
		return true;
	}

	if (!bT0Valid && !bT1Valid)
	{
		return false;
	}

	// looking for intersect ray and cone base plane
	if (rdAxis == 0)
	{
		return false;
	}

	float t2 = (h - coAxis) / rdAxis;
	float tv = bT0Valid ? t0 : t1;
	t0 = min(tv, t2);
	t1 = max(tv, t2);

	return true;
}

vec3 SpotLight_Volumetric(in SpotLight light, sampler2DShadow shadowMap,
#ifdef EG_TRANSLUCENT_SHADOWS
	sampler2D coloredTexture, sampler2D coloredDepthTexture,
#endif
	vec3 worldPos, vec3 cameraPos,
	vec3 normal, uint scatteringSamples, float scatteringZFar, bool bCastsShadow)
{
	const bool bVolumetricLight = light.VolumetricFogIntensity < 0;
	if (!bVolumetricLight)
		return vec3(0.f);

	const vec3 camToFrag = worldPos - cameraPos;
	const float camToFragLen = length(camToFrag);
	const vec3 camDir = camToFrag / camToFragLen;
	float t0, t1;

	if (!ConeIntersect(
		cameraPos,
		camDir,
		light.Position,
		light.Direction,
		light.Distance,
		cos(light.OuterCutOffRadians),
		t0, t1) || t1 < 0.f)
	{
		return vec3(0);
	}

	// Adjust sampling interval and origin
	t1 = min(t1, camToFragLen);
	t0 = clamp(t0, 0.0, t1);

	if (t0 == t1)
	{
		return vec3(0);
	}

	if (t1 < g_Near || t0 > camToFragLen || t0 > scatteringZFar)
	{
		return vec3(0);
	}

	const float deltaStep = (t1 - t0) / float(scatteringSamples);
	float currentT = (t0 + deltaStep * 0.5f);
	float tempT = currentT;

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
	
	for (uint i = 0; (i < scatteringSamples) && (currentT < camToFragLen); ++i)
	{
		vec3 currentPos = cameraPos + camDir * currentT;

		const vec3 incoming = light.Position - currentPos;
		const float distance2 = dot(incoming, incoming);
		const float incomingLen = sqrt(distance2);
		const vec3 normIncoming = incoming / incomingLen;
		const float theta = clamp(dot(normIncoming, normSpotDir), 0.0, 1.0);
		const float cutoffIntensity = clamp((theta - outerCutOffCos) / epsilon, 0.0, 1.0);
		const float attenuation = cutoffIntensity / distance2;

		if (NOT_ZERO(attenuation))
		{
			float visibility = bCastsShadow ? 0.f : 1.f;
#ifdef EG_TRANSLUCENT_SHADOWS
			vec3 coloredShadow = vec3(1.f);
#endif
			if (bCastsShadow)
			{
				const mat4 lightVP = g_LightMatrices[light.ViewProjOffset];
				const vec3 lightSpacePos = GetSpotLightSpacePosition(shadowMap, light, lightVP, currentPos, normIncoming, normIncoming, incomingLen);

				visibility = SpotLight_ShadowCalculation_Volumetric(shadowMap, lightSpacePos);
#ifdef EG_TRANSLUCENT_SHADOWS
				coloredShadow = SpotLight_ColoredShadowCalculation_Volumetric(coloredTexture, coloredDepthTexture, lightSpacePos);
#endif
			}
#ifdef EG_VOLUMETRIC_FOG
			float fog = SampleFog(currentPos);
#else
			float fog = 1.f;
#endif

#ifdef EG_TRANSLUCENT_SHADOWS
			result += fog * coloredShadow * visibility * attenuation * PhaseFunction(g_FogAnisotropy, dot(normIncoming, camDir));
#else
			result += fog * visibility * attenuation * PhaseFunction(g_FogAnisotropy, dot(normIncoming, camDir));
#endif
		}

		tempT += deltaStep;

		const float rand = DITHER_PATTERN[i % 16];
		const float jitter = (rand - 0.5) * deltaStep;
		currentT = tempT + jitter;
	}

	return result / scatteringSamples * light.LightColor * abs(light.VolumetricFogIntensity);
}

#endif
