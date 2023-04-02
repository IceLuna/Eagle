#include "pipeline_layout.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out int  outObjectID;

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_TextureIndex;
layout(location = 2) flat in int  i_EntityID;

void main()
{
    outAlbedo = ReadTexture(i_TextureIndex, i_TexCoords);
    outObjectID = i_EntityID;
}
