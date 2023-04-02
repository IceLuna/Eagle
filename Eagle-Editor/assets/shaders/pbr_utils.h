#ifndef EG_PBR_UTILS
#define EG_PBR_UTILS

#include "defines.h"

// Trowbridge-Reitz GGX normal distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	const float a = roughness * roughness;
	const float a2 = a * a;
	
	const float NdotH = max(dot(N, H), FLT_SMALL);
	const float NdotH2 = NdotH * NdotH;

	float denom = NdotH2 * (a2 - 1) + 1;
	denom = EG_PI * denom * denom;

	return a2 / max(denom, FLT_SMALL);
}

float GeometrySchlickGGX(float NdotV, float k)
{
	float denom = NdotV * (1.f - k) + k;
	return NdotV / denom;
}

float GeometrySmith(float NdotV, float NdotL, float k)
{
	const float ggx1 = GeometrySchlickGGX(NdotV, k);
	const float ggx2 = GeometrySchlickGGX(NdotL, k);
	return ggx1 * ggx2;
}

float GeometryFunction(float NdotV, float NdotL, float roughness)
{
	float k = (roughness + 1.f);
	k = (k * k) / 8.f;

	return GeometrySmith(NdotV, NdotL, k);
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
	float a = roughness * roughness;

	float phi = 2.0 * EG_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
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

		const float VdotH = max(dot(V, H), FLT_SMALL);
		const vec3 L = normalize(2.0 * VdotH * H - V);

		const float NdotL = max(L.z, 0.0);
		const float NdotH = max(H.z, FLT_SMALL);

		if (NdotL > 0.0)
		{
			const float G = GeometryFunctionIBL(NdotV, NdotL, roughness);
			const float G_Vis = (G * VdotH * max(NdotL, FLT_SMALL)) / (NdotH);
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
	vec3 L = normalize(incoming);
	vec3 H = normalize(L + V);

	float distance = length(incoming);
	float attenuation = 1.f / (distance * distance);
	vec3 radiance = lightIntensity * lightColor * attenuation;

	const float VdotH = max(dot(V, H), FLT_SMALL);
	const float NdotV = max(dot(N, V), FLT_SMALL);
	const float NdotL = max(dot(N, L), FLT_SMALL);

	vec3 F = FresnelSchlick(F0, VdotH);
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometryFunction(NdotV, NdotL, roughness);
	vec3 numerator = NDF * G * F;
	float denominator = 4.f * NdotV * NdotL + 0.0001f;

	vec3 specular = numerator / denominator;

	vec3 kS = F;
	vec3 kD = vec3(1.f) - F;
	kD *= (1.f - metallness);

	return (kD * lambert_albedo + specular) * radiance * NdotL;
}

#endif
