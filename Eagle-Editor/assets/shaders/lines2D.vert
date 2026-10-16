#include "utils.h"

layout(location = 0) in vec4  a_Color;
layout(location = 1) in vec2  a_Anchor;
layout(location = 2) in vec2  a_Offset;   // Reference pixels
layout(location = 3) in vec2  a_Position; // Reference pixels
layout(location = 4) in float a_Rotation; // Radians

layout(location = 0) out vec4 o_Color;

layout(push_constant) uniform PushConstants
{
    vec2 g_ViewportSize;
    float g_UIScale; // Size of a reference pixel in viewport pixels
};

void main()
{
    gl_Position = UI2D_GetClipPosition(g_ViewportSize, g_UIScale, a_Anchor, a_Offset, a_Position, a_Rotation);
    o_Color = a_Color;
}
