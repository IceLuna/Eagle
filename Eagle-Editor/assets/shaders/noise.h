#ifndef EG_NOISE_H
#define EG_NOISE_H

// Procedural noise functions
// The noise repeats every `Noise_Period` lattice cells along each axis. So an offset added to the sample position (to animate the noise, for example)
// can be wrapped with `mod(offset, float(Noise_Period))`.
const uint Noise_Period = 256u;

// Pseudo-random gradient in [-1; 1]^3 for an integer lattice point (integer hash, no sin/fract).
// Periodic. Lattice points `Noise_Period` apart get the same gradient
vec3 Noise_Hash3(vec3 latticePoint)
{
	const uvec3 k = uvec3(1597334673u, 3812015801u, 2798796415u);
	uvec3 q = (uvec3(ivec3(latticePoint)) & (Noise_Period - 1u)) * k;
	q = (q.x ^ q.y ^ q.z) * k;
	return -1.f + 2.f * vec3(q) * (1.f / float(0xffffffffu)); // Gradient in [-1; 1]
}

// 3D gradient noise with analytic derivatives (quintic interpolation, so the derivatives are smooth too).
// Returns (value, d/dx, d/dy, d/dz). The value is roughly in [-1; 1]. See https://iquilezles.org/articles/gradientnoise/
vec4 Noise_GradientWithDerivatives(vec3 x)
{
	const vec3 i = floor(x);
	const vec3 f = x - i;
	const vec3 u = f * f * f * (f * (f * 6.f - 15.f) + 10.f);
	const vec3 du = 30.f * f * f * (f * (f - 2.f) + 1.f);

	const vec3 ga = Noise_Hash3(i + vec3(0.f, 0.f, 0.f));
	const vec3 gb = Noise_Hash3(i + vec3(1.f, 0.f, 0.f));
	const vec3 gc = Noise_Hash3(i + vec3(0.f, 1.f, 0.f));
	const vec3 gd = Noise_Hash3(i + vec3(1.f, 1.f, 0.f));
	const vec3 ge = Noise_Hash3(i + vec3(0.f, 0.f, 1.f));
	const vec3 gf = Noise_Hash3(i + vec3(1.f, 0.f, 1.f));
	const vec3 gg = Noise_Hash3(i + vec3(0.f, 1.f, 1.f));
	const vec3 gh = Noise_Hash3(i + vec3(1.f, 1.f, 1.f));

	const float va = dot(ga, f - vec3(0.f, 0.f, 0.f));
	const float vb = dot(gb, f - vec3(1.f, 0.f, 0.f));
	const float vc = dot(gc, f - vec3(0.f, 1.f, 0.f));
	const float vd = dot(gd, f - vec3(1.f, 1.f, 0.f));
	const float ve = dot(ge, f - vec3(0.f, 0.f, 1.f));
	const float vf = dot(gf, f - vec3(1.f, 0.f, 1.f));
	const float vg = dot(gg, f - vec3(0.f, 1.f, 1.f));
	const float vh = dot(gh, f - vec3(1.f, 1.f, 1.f));

	const float k0 = va;
	const float k1 = vb - va;
	const float k2 = vc - va;
	const float k3 = ve - va;
	const float k4 = va - vb - vc + vd;
	const float k5 = va - vc - ve + vg;
	const float k6 = va - vb - ve + vf;
	const float k7 = -va + vb + vc - vd + ve - vf - vg + vh;

	const vec3 g0 = ga;
	const vec3 g1 = gb - ga;
	const vec3 g2 = gc - ga;
	const vec3 g3 = ge - ga;
	const vec3 g4 = ga - gb - gc + gd;
	const vec3 g5 = ga - gc - ge + gg;
	const vec3 g6 = ga - gb - ge + gf;
	const vec3 g7 = -ga + gb + gc - gd + ge - gf - gg + gh;

	const float value = k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
	const vec3 gradient = g0 + g1 * u.x + g2 * u.y + g3 * u.z + g4 * u.x * u.y + g5 * u.y * u.z + g6 * u.z * u.x + g7 * u.x * u.y * u.z
		+ du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
		            k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
		            k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
	return vec4(value, gradient);
}

// Curl noise. The curl of a vector potential made of three gradient noise fields.
// The result is a smooth, divergence-free vector field. Things that follow it swirl around instead of bunching up or spreading out.
// Its length is around 1 on average (up to about 3)
vec3 Noise_Curl(vec3 p)
{
	const vec3 gx = Noise_GradientWithDerivatives(p).yzw;
	const vec3 gy = Noise_GradientWithDerivatives(p + vec3(31.341f, -43.23f, 12.34f)).yzw;
	const vec3 gz = Noise_GradientWithDerivatives(p + vec3(-231.341f, 124.23f, -54.34f)).yzw;
	return vec3(gz.y - gy.z, gx.z - gz.x, gy.x - gx.y);
}

#endif // EG_NOISE_H
