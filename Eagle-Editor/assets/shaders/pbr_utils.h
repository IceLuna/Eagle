#ifndef EG_PBR_UTILS
#define EG_PBR_UTILS

#include "defines.h"

#define BRDF_DIFFUSE_MODEL_LAMBERT 0
#define BRDF_DIFFUSE_MODEL_BURLEY 1

#define BRDF_DIFFUSE_MODEL BRDF_DIFFUSE_MODEL_BURLEY

float pow5(float x)
{
	const float x2 = x * x;
	return x2 * x2 * x;
}

float RoughnessToAlpha(float roughness)
{
	return roughness * roughness;
}

// Trowbridge-Reitz GGX normal distribution function
// moving_frostbite_to_pbr_v32: page 12
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	const float a = RoughnessToAlpha(roughness);
	const float a2 = a * a;
	
	const float NdotH = clamp(dot(N, H), 0.0, 1.0);

	const float f = (NdotH * a2 - NdotH) * NdotH + 1;
	return a2 / max(f * f * EG_PI, FLT_EPSILON);
}

float GeometrySchlickGGX(float cosTheta, float k)
{
	float denom = cosTheta * (1.f - k) + k;
	return cosTheta / max(denom, EG_FLT_SMALL);
}

float GeometryFunction(float NdotV, float NdotL, float roughness)
{
#if 1 // Height corrected
	const float a = RoughnessToAlpha(roughness);
	const float a2 = a * a;

	const float ggx1 = NdotL * sqrt(a2 + ((1.0f - a2) * NdotV * NdotV));
	const float ggx2 = NdotV * sqrt(a2 + ((1.0f - a2) * NdotL * NdotL));
	return 0.5 / max(ggx1 + ggx2, FLT_EPSILON);
#else
	float k = (roughness + 1.f);
	k = (k * k) / 8.f;
	
	const float ggx1 = GeometrySchlickGGX(NdotV, k);
	const float ggx2 = GeometrySchlickGGX(NdotL, k);
	const float denominator = 4.f * NdotV * NdotL;

	return (ggx1 * ggx2) / max(denominator, FLT_EPSILON);
#endif
}

float FresnelSchlick(float F0, float F90, float cosTheta)
{
	return F0 + (F90 - F0) * pow5(clamp(1.f - cosTheta, 0.f, 1.f));
}

vec3 FresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (1.f - F0) * pow5(clamp(1.f - cosTheta, 0.f, 1.f));
}

vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness)
{
	return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow5(clamp(1.f - cosTheta, 0.f, 1.f));
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
	const vec3 bitangent = normalize(cross(N, tangent));
	
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

vec3 BRDF_Diffuse_Lambert(vec3 albedo)
{
	return albedo * EG_INV_PI;
}

vec3 BRDF_Diffuse_Burley(vec3 albedo, float roughness, float VdotH, float NdotL, float NdotV)
{
	float F90 = 0.5f + 2.0f * roughness * VdotH * VdotH;
	float lightScatter = FresnelSchlick(1.0, F90, NdotL);
	float viewScatter = FresnelSchlick(1.0, F90, NdotV);
	return albedo * (lightScatter * viewScatter * EG_INV_PI);
}

vec3 BRDF_Diffuse(vec3 albedo, float roughness, float VdotH, float NdotL, float NdotV)
{
#if BRDF_DIFFUSE_MODEL == BRDF_DIFFUSE_MODEL_LAMBERT
	return BRDF_Diffuse_Lambert(albedo);
#else
	return BRDF_Diffuse_Burley(albedo, roughness, VdotH, NdotL, NdotV);
#endif
}

vec3 EvaluatePBR(vec3 albedo, vec3 incoming, vec3 V, vec3 N, vec3 F0, float metallness, float roughness, vec3 lightColor, float lightIntensity)
{
	const vec3 L = incoming;
	const vec3 H = normalize(L + V);
	
	const vec3 radiance = lightIntensity * lightColor;
	
	const float VdotH = clamp(dot(V, H), 0.0, 1.0);
	const float NdotV = clamp(dot(N, V), 0.0, 1.0);
	const float NdotL = clamp(dot(N, L), 0.0, 1.0);
	
	const vec3 F = FresnelSchlick(F0, VdotH);
	const float NDF = DistributionGGX(N, H, roughness);
	const float G = GeometryFunction(NdotV, NdotL, roughness);

	const vec3 specularBRDF = NDF * G * F;
	const vec3 diffuseBRDF = BRDF_Diffuse(albedo, roughness, VdotH, NdotL, NdotV);

	const vec3 kS = F;
	vec3 kD = vec3(1.f) - kS;
	kD *= (1.f - metallness);

	return (kD * diffuseBRDF + specularBRDF) * radiance * NdotL;
}

#ifdef PBR_EVALUATE_IBL
// @viewDir. fragment to camera
vec3 EvaluateIBL(vec3 albedo, vec3 F0, vec3 normal, vec3 bentNormal, vec3 viewDir, float roughness, float metalness, float maxReflectionLOD)
{
	const vec3 R = reflect(-viewDir, normal);
	const float NdotV = clamp(dot(normal, viewDir), 0.0, 1.0);

	const vec3 Fr = max(vec3(1.f - roughness), F0) - F0;
	const vec3 F = F0 + Fr * pow5(1.f - NdotV);
	const vec3 kS = F;
	const vec3 kD = (1.0 - kS) * (1.0 - metalness);

	const vec3 prefilteredColor = textureLod(g_PrefilterMap, R, roughness * maxReflectionLOD).rgb;
	const vec2 brdf = texture(g_BRDFLUT, vec2(NdotV, roughness)).rg;
	const vec3 ambientDiffuse = kD * albedo * texture(g_IrradianceMap, bentNormal).rgb;

	const float E_o = brdf.x + brdf.y;
	const vec3 envSpecBRDFss = F * brdf.x + brdf.y;
	const vec3 multiScatterScale = F0 * (1.0 - E_o) / E_o + vec3(1.0); // Multiple Scattering (for ambient light)
	const vec3 ambientSpecular = prefilteredColor * multiScatterScale * envSpecBRDFss;

	return ambientDiffuse + ambientSpecular;
}

// @viewDir. fragment to camera
vec3 EvaluateIBL(vec3 albedo, vec3 F0, vec3 normal, vec3 viewDir, float roughness, float metalness, float maxReflectionLOD)
{
	return EvaluateIBL(albedo, F0, normal, normal, viewDir, roughness, metalness, maxReflectionLOD);
}
#endif

#endif
