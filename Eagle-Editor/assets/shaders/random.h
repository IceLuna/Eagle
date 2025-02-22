#ifndef EG_RANDOM
#define EG_RANDOM

#include "defines.h"

// Random number generator based on: https://github.com/diharaw/helios/blob/master/src/engine/shader/random.glsl
struct Random
{
    uvec2 s; // state
};

// xoroshiro64* random number generator.
// http://prng.di.unimi.it/xoroshiro64star.c
uint Random_Rotl(uint x, uint k)
{
    return (x << k) | (x >> (32 - k));
}

// Xoroshiro64* RNG
uint Random_Next(inout Random random)
{
    uint result = random.s.x * 0x9e3779bb;

    random.s.y ^= random.s.x;
    random.s.x = Random_Rotl(random.s.x, 26) ^ random.s.y ^ (random.s.y << 9);
    random.s.y = Random_Rotl(random.s.y, 13);

    return result;
}

// Thomas Wang 32-bit hash.
// http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint Random_Hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

Random Random_Init(uvec2 id, uint frameIndex)
{
    Random random;
    uint s0 = (id.x << 16) | id.y;
    uint s1 = frameIndex;
    random.s.x = Random_Hash(s0);
    random.s.y = Random_Hash(s1);
    Random_Next(random);

    return random;
}

float Random_NextFloat(inout Random random)
{
    uint u = 0x3f800000 | (Random_Next(random) >> 9);
    return uintBitsToFloat(u) - 1.0;
}

vec2 Random_NextFloat2(inout Random random)
{
    return vec2(Random_NextFloat(random), Random_NextFloat(random));
}

vec3 Random_NextFloat3(inout Random random)
{
    return vec3(Random_NextFloat(random), Random_NextFloat(random), Random_NextFloat(random));
}

uint Random_NextUint(inout Random random, uint nmax)
{
    float f = Random_NextFloat(random);
    return uint(floor(f * nmax));
}

vec3 Random_PointInSphere(inout Random random, vec3 radius)
{
    if (IS_ZERO(radius))
        return vec3(0);

    // Rejection sampling method
    vec3 point;
    do {
        point.x = (radius.x != 0.0f) ? Random_NextFloat(random) * 2.f - 1.f : 0.0f;
        point.y = (radius.y != 0.0f) ? Random_NextFloat(random) * 2.f - 1.f : 0.0f;
        point.z = (radius.z != 0.0f) ? Random_NextFloat(random) * 2.f - 1.f : 0.0f;
    } while (dot(point, point) > 1.0f); // Ensure it's inside unit sphere

    return point * radius;
}

vec3 Random_PointOnSphere(inout Random random, vec3 radius)
{
    float theta = 2 * EG_PI * Random_NextFloat(random);
    float phi = acos(2 * Random_NextFloat(random) - 1);
    float x = radius.x * sin(phi) * cos(theta);
    float y = radius.y * sin(phi) * sin(theta);
    float z = radius.z * cos(phi);
    return vec3(x, y, z);
}

vec3 Random_PointInRing(inout Random random, vec3 ringRadius, vec3 thickness)
{
    // Step 1: Generate a random point within the circular cross-section of the tube
    vec3 r_inner = sqrt(Random_NextFloat3(random) * thickness * thickness); // Random radius within the tube
    float theta_inner = Random_NextFloat(random) * 2 * EG_PI; // Random angle within the tube

    // Convert to Cartesian coordinates within the tube cross - section
    vec3 x_inner = r_inner * cos(theta_inner);
    vec3 y_inner = r_inner * sin(theta_inner);

    // Step 2: Generate a random angle around the major circle of the torus
    float theta_outer = Random_NextFloat(random) * 2 * EG_PI;

    // Step 3: Convert to 3D Cartesian coordinates
    float x = (ringRadius.x + x_inner.x) * cos(theta_outer);
    float y = (ringRadius.y + x_inner.y) * sin(theta_outer);
    float z = y_inner.x * ringRadius.z * 10.f;

    return vec3(x, y, z);
}

#endif
