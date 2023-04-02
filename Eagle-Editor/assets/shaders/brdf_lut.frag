#include "defines.h"
#include "pbr_utils.h"

layout(location = 0) in vec2 i_UV;

layout(location = 0) out vec2 outColor;

void main()
{		
    outColor = IntegrateBRDF(max(i_UV.x, FLT_SMALL), max(i_UV.y, EG_MIN_ROUGHNESS));
}
