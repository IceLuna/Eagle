#include "pipeline_layout.h"

layout(location = 0) out vec4 outAlbedo;

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_TextureIndex;

void main()
{
    outAlbedo = ReadTexture(i_TextureIndex, i_TexCoords);
}
