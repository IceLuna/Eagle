#ifndef EG_VOLUMETRICS_COMMON
#define EG_VOLUMETRICS_COMMON

#include "defines.h"
#include "utils.h"

// Two depths are considered to belong to the same surface if they differ by less than this fraction of the reference depth.
// It's used to prevent volumetric light from leaking across depth discontinuities when blurring and upsampling the low-res result.
#define EG_VOLUMETRIC_DEPTH_TOLERANCE 0.2f

// Positive distance from the camera plane
float Volumetric_GetLinearDepth(mat4 projInv, vec2 uv, float depth)
{
    return -ViewPosFromDepth(projInv, uv, depth).z;
}

float Volumetric_GetRelativeDepthDifference(float depth, float referenceDepth)
{
    return abs(depth - referenceDepth) / max(referenceDepth, EG_FLT_SMALL);
}

#endif
