#include "utils.h"

layout(location = 0) in vec3  a_Color;
layout(location = 1) in uint  a_AtlasIndex;
layout(location = 2) in vec2  a_Position; // Relative to the pivot, reference pixels
layout(location = 3) in vec2  a_TexCoords;
layout(location = 4) in int   a_EntityID;
layout(location = 5) in float a_Opacity;
layout(location = 6) in vec2  a_Anchor;
layout(location = 7) in vec2  a_Offset;   // Reference pixels
layout(location = 8) in float a_Rotation; // Radians
layout(location = 9) in uint  a_EffectsIndex; // Index into the effects buffer. The highest bit is set for shadow quads

layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_AtlasIndex;
layout(location = 3) flat out float o_Opacity;
#ifndef EG_NO_OBJECT_ID
layout(location = 4) flat out int o_EntityID;
#endif
layout(location = 5) flat out uint o_EffectsIndex;

layout(push_constant) uniform PushConstants
{
    vec2 g_ViewportSize;
    float g_UIScale; // Size of a reference pixel in viewport pixels
};

void main()
{
    gl_Position = UI2D_GetClipPosition(g_ViewportSize, g_UIScale, a_Anchor, a_Offset, a_Position, a_Rotation);

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_AtlasIndex = a_AtlasIndex;
    o_Opacity = a_Opacity;
    o_EffectsIndex = a_EffectsIndex;
#ifndef EG_NO_OBJECT_ID
    o_EntityID = a_EntityID;
#endif
}
