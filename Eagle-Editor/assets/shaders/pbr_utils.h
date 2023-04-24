#ifndef EG_PBR_UTILS
#define EG_PBR_UTILS

#include "defines.h"

// Trowbridge-Reitz GGX normal distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	const float a = roughness * roughness;
	const float a2 = a * a;
	
	const float NdotH = dot(N, H);
	const float NdotH2 = NdotH * NdotH;

	float denom = NdotH2 * (a2 - 1.f) + 1.f;
	denom = EG_PI * denom * denom;

	return max(a2, 0.01f) / max(denom, EG_FLT_SMALL);
}

float GeometrySchlickGGX(float cosTheta, float k)
{
	float denom = cosTheta * (1.f - k) + k;
	return cosTheta / max(denom, EG_FLT_SMALL);
}

float GeometryFunction(float NdotV, float NdotL, float roughness)
{
	float k = (roughness + 1.f);
	k = (k * k) / 8.f;

	const float ggx1 = GeometrySchlickGGX(NdotV, k);
	const float ggx2 = GeometrySchlickGGX(NdotL, k);
	return ggx1 * ggx2;
}

float GeometryFunctionIBL(float NdotV, float NdotL, float roughness)
{
	float a2 = pow(roughness, 4.0);
	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	return 0.5 / (GGXV + GGXL);
}

vec3 FresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (1.f - F0) * pow(clamp(1.f - cosTheta, 0.f, 1.f), 5.f);
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow(clamp(1.f - cosTheta, 0.f, 1.f), 5.f);
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	const float a = roughness * roughness;

	const float phi = 2.0 * EG_PI * Xi.x;
	const float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	const vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	const vec3 tangent = normalize(cross(up, N));
	const vec3 bitangent = cross(N, tangent);

	const vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
	const vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.f, NdotV);

	float A = 0.0;
	float B = 0.0;

	const vec3 N = vec3(0.0, 0.0, 1.0);

	const uint SAMPLE_COUNT = 1024u;
	const float ONE_OVER_SAMPLE_COUNT = 1.f / float(SAMPLE_COUNT);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		// generates a sample vector that's biased towards the
		// preferred alignment direction (importance sampling).
		const vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		const vec3 H = ImportanceSampleGGX(Xi, N, roughness);

		const float VdotH = clamp(dot(V, H), EG_FLT_SMALL, 1.0);
		const vec3 L = normalize(2.0 * VdotH * H - V);

		const float NdotL = clamp(L.z, 0.0, 1.0);
		const float NdotH = clamp(H.z, EG_FLT_SMALL, 1.0);

		if (NdotL > 0.0)
		{
			const float G = GeometryFunctionIBL(NdotV, NdotL, roughness);
			const float G_Vis = (G * VdotH * max(NdotL, EG_FLT_SMALL)) / NdotH;
			const float Fc = pow(1.0 - VdotH, 5.0);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	A *= ONE_OVER_SAMPLE_COUNT;
	B *= ONE_OVER_SAMPLE_COUNT;

	return 4.f * vec2(A, B);
}

vec3 EvaluatePBR(vec3 lambert_albedo, vec3 incoming, vec3 V, vec3 N, vec3 F0, float metallness, float roughness, vec3 lightColor, float lightIntensity)
{
	const vec3 L = incoming;
	const vec3 H = normalize(L + V);

	const vec3 radiance = lightIntensity * lightColor;

	const float VdotH = clamp(dot(V, H), EG_FLT_SMALL, 1.0);
	const float NdotV = clamp(dot(N, V), EG_FLT_SMALL, 1.0);
	const float NdotL = clamp(dot(N, L), EG_FLT_SMALL, 1.0);

	const vec3 F = FresnelSchlick(F0, VdotH);
	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometryFunction(NdotV, NdotL, roughness);
	const vec3 numerator = NDF * G * F;
	const float denominator = 4.f * NdotV * NdotL + EG_FLT_SMALL;

	const vec3 specular = numerator / denominator;

	const vec3 kS = F;
	vec3 kD = vec3(1.f) - kS;
	kD *= (1.f - metallness);

	return (kD * lambert_albedo + specular) * radiance * NdotL;
}

#endif
