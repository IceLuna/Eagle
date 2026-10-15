#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "text/msdf.h"

layout(location = 0) in vec3 i_Color;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in int i_EntityID;
layout(location = 3) flat in uint i_AtlasIndex;
#ifdef EG_MOTION
layout(location = 4) in vec3 i_CurPos;
layout(location = 5) in vec3 i_PrevPos;
#endif

layout(location = 0) out vec4 outColor;
layout(location = 1) out int  outObjectID;
#ifdef EG_MOTION
layout(location = 2) out vec2 outMotion;
#endif

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[];

void main()
{
	const float opacity = MSDF_COVERAGE(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);

    if (opacity < 0.5f)
        discard;

    outColor = vec4(i_Color, opacity);
    outObjectID = i_EntityID;

#ifdef EG_MOTION
    outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
