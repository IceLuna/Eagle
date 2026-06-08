#ifndef SKIN_CACHE_UTILS
#define SKIN_CACHE_UTILS

struct AABB
{
    vec3 Min;
    vec3 Max;
};

struct UAABB
{
    uvec3 Min;
    uvec3 Max;
};

uint floatToUintSortable(float f)
{
    uint u = floatBitsToUint(f);
    return (u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u);
}

uvec3 floatToUintSortable(vec3 f)
{
    uvec3 result;
    for (int i = 0; i < 3; ++i)
        result[i] = floatToUintSortable(f[i]);

    return result;
}

float uintSortableToFloat(uint u)
{
    u = (u & 0x80000000u) == 0u ? ~u : (u & ~0x80000000u);
    return uintBitsToFloat(u);
}

vec3 uintSortableToFloat(uvec3 u)
{
    vec3 result;
    for (int i = 0; i < 3; ++i)
        result[i] = uintSortableToFloat(u[i]);

    return result;
}

#define doAtomicMin(mem, data) \
{ \
    for (int i = 0; i < 3; ++i) \
    { \
        atomicMin(mem[i], data[i]); \
    } \
}

#define doAtomicMax(mem, data) \
{ \
    for (int i = 0; i < 3; ++i) \
    { \
        atomicMax(mem[i], data[i]); \
    } \
}

#endif
