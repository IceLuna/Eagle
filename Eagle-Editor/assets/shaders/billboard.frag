#include "pipeline_layout.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out int  outObjectID;

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_TextureIndex;
layout(location = 2) flat in int  i_EntityID;

void main()
{
    const vec4 color = ReadTexture(i_TextureIndex, i_TexCoords);

    // Alpha mask
    if (color.a < 0.9f)
        discard;

    outAlbedo = color;
    outObjectID = i_EntityID;
}
