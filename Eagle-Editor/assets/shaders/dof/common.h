#ifndef EG_DOF_COMMON
#define EG_DOF_COMMON

#include "defines.h"

#define GROUP_SIZE 8
#define DOF_TILESIZE 32
#define DOF_MAX_RING_COUNT 4
#define DOF_RING_COUNT 3
#define DOF_TILE_REPLICATE (EG_SQUARE(DOF_TILESIZE / 2 / GROUP_SIZE))

layout(push_constant) uniform PushConstants
{
    vec2 g_TexelSize;
    vec2 g_ApertureShape;
    float g_ApertureSize;
    float g_FocalLength;
    float g_ZNear;
    float g_ZFar;
    float g_COCScale;
    float g_MaxCOC;
    uvec2 g_Size;
    uvec2 g_PassSize;
};

float ToLinear(float d)
{
    return g_ZNear * g_ZFar / (g_ZFar + d * (g_ZNear - g_ZFar));
}

float GetCOC(float depth)
{
    return min(g_MaxCOC, g_COCScale * g_ApertureSize * pow(abs(1 - g_FocalLength / (depth)), 2.0f));
}

vec2 DepthCmp2(float depth, float closestTileDepth)
{
    const float depthScaleForeground = 1.5f;
    float d = depthScaleForeground * (depth - closestTileDepth);
    vec2 depthCmp;
    depthCmp.x = smoothstep(0.0, 1.0, d); // Background
    depthCmp.y = 1.0 - depthCmp.x; // Foreground
    return depthCmp;
}

float SampleAlpha(float sampleCoc)
{
    const float singlePixelRadius = 0.7071f; // length( float2(0.5, 0.5 ) );
    return min(1.f / (EG_PI * sampleCoc * sampleCoc), 1.f / (EG_PI * singlePixelRadius * singlePixelRadius));
}

float SpreadToe(float offsetCoc, float spreadCmp)
{
    const float spreadToePower = 2.f;
    return offsetCoc <= 1.f ? pow(spreadCmp, spreadToePower) : spreadCmp;
}
float SpreadCmp(float offsetCoc, float sampleCoc, float pixelToSampleUnitsScale)
{
    return SpreadToe(offsetCoc, clamp(pixelToSampleUnitsScale * sampleCoc - offsetCoc + 1.f, 0.f, 1.f));
}

uvec2 Unflatten2D(uint idx, uvec2 dim)
{
    return uvec2(idx % dim.x, idx / dim.x);
}

const vec3 g_DOFDisk[] =
{
    vec3(  0.2310, -0.0957, 1.0 ),
    vec3( -0.2310,  0.0957, 1.0 ),
    vec3( -0.2310, -0.0957, 1.0 ),
    vec3(  0.2310,  0.0957, 1.0 ),
    vec3( -0.0957, -0.2310, 1.0 ),
    vec3(  0.0957,  0.2310, 1.0 ),
    vec3(  0.0957, -0.2310, 1.0 ),
    vec3( -0.0957,  0.2310, 1.0 ),
    vec3( -0.3536,  0.3536, 2.0 ),
    vec3(  0.3536, -0.3536, 2.0 ),
    vec3(  0.4619, -0.1913, 2.0 ),
    vec3( -0.4619,  0.1913, 2.0 ),
    vec3( -0.5000,  0.0000, 2.0 ),
    vec3(  0.5000,  0.0000, 2.0 ),
    vec3( -0.4619, -0.1913, 2.0 ),
    vec3(  0.4619,  0.1913, 2.0 ),
    vec3( -0.3536, -0.3536, 2.0 ),
    vec3(  0.3536,  0.3536, 2.0 ),
    vec3( -0.1913, -0.4619, 2.0 ),
    vec3(  0.1913,  0.4619, 2.0 ),
    vec3( -0.0000, -0.5000, 2.0 ),
    vec3(  0.0000,  0.5000, 2.0 ),
    vec3(  0.1913, -0.4619, 2.0 ),
    vec3( -0.1913,  0.4619, 2.0 ),
    vec3(  0.4566, -0.5950, 3.0 ),
    vec3( -0.4566,  0.5950, 3.0 ),
    vec3( -0.5950,  0.4566, 3.0 ),
    vec3(  0.5950, -0.4566, 3.0 ),
    vec3(  0.6929, -0.2870, 3.0 ),
    vec3( -0.6929,  0.2870, 3.0 ),
    vec3( -0.7436,  0.0979, 3.0 ),
    vec3(  0.7436, -0.0979, 3.0 ),
    vec3( -0.7436, -0.0979, 3.0 ),
    vec3(  0.7436,  0.0979, 3.0 ),
    vec3( -0.6929, -0.2870, 3.0 ),
    vec3(  0.6929,  0.2870, 3.0 ),
    vec3( -0.5950, -0.4566, 3.0 ),
    vec3(  0.5950,  0.4566, 3.0 ),
    vec3(  0.4566,  0.5950, 3.0 ),
    vec3( -0.4566, -0.5950, 3.0 ),
    vec3( -0.2870, -0.6929, 3.0 ),
    vec3(  0.2870,  0.6929, 3.0 ),
    vec3( -0.0979, -0.7436, 3.0 ),
    vec3(  0.0979,  0.7436, 3.0 ),
    vec3( -0.0979,  0.7436, 3.0 ),
    vec3(  0.0979, -0.7436, 3.0 ),
    vec3(  0.2870, -0.6929, 3.0 ),
    vec3( -0.2870,  0.6929, 3.0 ),
    vec3( -0.5556,  0.8315, 4.0 ),
    vec3(  0.5556, -0.8315, 4.0 ),
    vec3( -0.7071,  0.7071, 4.0 ),
    vec3(  0.7071, -0.7071, 4.0 ),
    vec3(  0.8315, -0.5556, 4.0 ),
    vec3( -0.8315,  0.5556, 4.0 ),
    vec3(  0.9239, -0.3827, 4.0 ),
    vec3( -0.9239,  0.3827, 4.0 ),
    vec3(  0.9808, -0.1951, 4.0 ),
    vec3( -0.9808,  0.1951, 4.0 ),
    vec3( -1.0000,  0.0000, 4.0 ),
    vec3(  1.0000,  0.0000, 4.0 ),
    vec3( -0.9808, -0.1951, 4.0 ),
    vec3(  0.9808,  0.1951, 4.0 ),
    vec3( -0.9239, -0.3827, 4.0 ),
    vec3(  0.9239,  0.3827, 4.0 ),
    vec3(  0.8315,  0.5556, 4.0 ),
    vec3( -0.8315, -0.5556, 4.0 ),
    vec3( -0.7071, -0.7071, 4.0 ),
    vec3(  0.7071,  0.7071, 4.0 ),
    vec3( -0.5556, -0.8315, 4.0 ),
    vec3(  0.5556,  0.8315, 4.0 ),
    vec3( -0.3827, -0.9239, 4.0 ),
    vec3(  0.3827,  0.9239, 4.0 ),
    vec3(  0.1951,  0.9808, 4.0 ),
    vec3( -0.1951, -0.9808, 4.0 ),
    vec3( -0.0000, -1.0000, 4.0 ),
    vec3(  0.0000,  1.0000, 4.0 ),
    vec3(  0.1951, -0.9808, 4.0 ),
    vec3( -0.1951,  0.9808, 4.0 ),
    vec3(  0.3827, -0.9239, 4.0 ),
    vec3( -0.3827,  0.9239, 4.0 ),
};

const uint g_DOFRingSampleCount[] = { 0, 8, 24, 48, 80 };
const float g_DOFRingNormFactor[] = { 1.0, 0.111111, 0.040000, 0.020408, 0.012346 };

#endif // EG_DOF_COMMON
