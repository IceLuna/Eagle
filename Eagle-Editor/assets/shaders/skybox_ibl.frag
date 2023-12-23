#include "defines.h"

layout(location = 0) in vec3 i_Pos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_CubeMap;

void main()
{
    const vec3 color = texture(u_CubeMap, i_Pos).rgb;
    outColor = vec4(color, 1.0);
}
