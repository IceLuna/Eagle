#include "defines.h"
#include "text/msdf.h"

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) flat in int i_EntityID;
layout(location = 1) flat in uint i_AtlasIndex;
layout(location = 2) in vec2 i_TexCoords;

layout(location = 0) out int outEntityID;

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[];

void main()
{
    const float fadeoutOpacity = MSDF_COVERAGE(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);

    if (fadeoutOpacity < 0.5f)
    {
        discard;
        return;
    }

    outEntityID = i_EntityID;
}
