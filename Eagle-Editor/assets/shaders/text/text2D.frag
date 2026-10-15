#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "text/msdf.h"

layout(location = 0) in vec3 i_Color;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in uint i_AtlasIndex;
layout(location = 3) flat in float i_Opacity;
#ifndef EG_NO_OBJECT_ID
layout(location = 4) flat in int i_EntityID;
#endif

layout(location = 0) out vec4 outColor;
#ifndef EG_NO_OBJECT_ID
layout(location = 1) out int  outObjectID;
#endif

layout(binding = 0) uniform sampler2D g_FontAtlases[];

void main()
{
    const float opacity = i_Opacity * MSDF_COVERAGE(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);

    if (IS_ZERO(opacity))
        discard;

    outColor = vec4(i_Color, opacity);
#ifndef EG_NO_OBJECT_ID
    outObjectID = i_EntityID;
#endif
}
