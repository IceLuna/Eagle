#include "utils.h"

layout(location = 0) in vec3  a_Tint;
layout(location = 1) in uint  a_TextureIndex;
layout(location = 2) in vec2  a_Position;
layout(location = 3) in int   a_EntityID;
layout(location = 4) in float a_Opacity;
layout(location = 5) in vec2  a_Origin;   // NDC
layout(location = 6) in float a_Rotation;

layout(location = 0) out vec4 o_Tint_Opacity;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_TextureIndex;
#ifndef EG_NO_OBJECT_ID
layout(location = 3) flat out int o_EntityID;
#endif

const vec2 s_TexCoords[4] = {
    vec2(0.f, 1.f),
    vec2(1.f, 1.f),
    vec2(1.f, 0.f),
    vec2(0.f, 0.f)
};

layout(push_constant) uniform PushConstants
{
    vec2 g_ViewportSize;
};

void main()
{
    const vec2 halfViewport = 0.5f * g_ViewportSize;
    const vec2 offsetPx = Rotate2D(a_Position * halfViewport, a_Rotation);
    gl_Position = vec4(a_Origin + offsetPx / halfViewport, 0.f, 1.0);
    
    const uint vertexID = gl_VertexIndex % 4u;
    o_TexCoords = s_TexCoords[vertexID];
    o_Tint_Opacity = vec4(a_Tint, a_Opacity);
    o_TextureIndex = a_TextureIndex;
#ifndef EG_NO_OBJECT_ID
    o_EntityID = a_EntityID;
#endif
}
