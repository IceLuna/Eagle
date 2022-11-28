#include "defines.h"

// Trowbridge-Reitz GGX normal distribution function
float DistributionGGX(vec3 N, vec3 H, float a)
{
	const float a2 = a * a;
	
	float NdotH = dot(N, H);
	NdotH = NdotH < 0.f ? 0.f : NdotH; // max(0, NdotH)
	const float NdotH2 = NdotH * NdotH;

	float denom = NdotH2 * (a2 - 1) + 1;
	denom = PI * denom * denom;

	return a2 / denom;
}
