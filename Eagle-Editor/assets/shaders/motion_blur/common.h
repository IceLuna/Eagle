#ifndef EG_MOTIONBLUR_COMMON
#define EG_MOTIONBLUR_COMMON

#include "defines.h"
#include "utils.h"

#define GROUP_SIZE 8
#define MOTIONBLUR_TILESIZE 16
#define MOTIONBLUR_TILE_REPLICATE (EG_SQUARE(MOTIONBLUR_TILESIZE / GROUP_SIZE))

layout(push_constant) uniform PushConstants
{
    float g_Strength;
    float g_ZNear;
    float g_ZFar;
    uint g_Unused;
    vec2 g_TexelSize;
    uvec2 g_Size;
    uvec2 g_PassSize;
};

float ToLinear(float d)
{
    return ToLinear(d, g_ZNear, g_ZFar);
}

#endif // EG_MOTIONBLUR_COMMON
