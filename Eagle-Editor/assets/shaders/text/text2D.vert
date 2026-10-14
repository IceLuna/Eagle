#include "utils.h"

layout(location = 0) in vec3  a_Color;
layout(location = 1) in uint  a_AtlasIndex;
layout(location = 2) in vec2  a_Position;
layout(location = 3) in vec2  a_TexCoords;
layout(location = 4) in int   a_EntityID;
layout(location = 5) in float a_Opacity;
layout(location = 6) in vec2  a_Origin;   // NDC
layout(location = 7) in float a_Rotation;

layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_AtlasIndex;
layout(location = 3) flat out float o_Opacity;
#ifndef EG_NO_OBJECT_ID
layout(location = 4) flat out int o_EntityID;
#endif

layout(push_constant) uniform PushConstants
{
    vec2 g_ViewportSize;
};

void main()
{
    const vec2 halfViewport = 0.5f * g_ViewportSize;
    const vec2 offsetPx = Rotate2D(a_Position * halfViewport, a_Rotation);
    gl_Position = vec4(a_Origin + offsetPx / halfViewport, 0.f, 1.0);

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_AtlasIndex = a_AtlasIndex;
    o_Opacity = a_Opacity;
#ifndef EG_NO_OBJECT_ID
    o_EntityID = a_EntityID;
#endif
}
