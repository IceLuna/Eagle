#include "pipeline_layout.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out int  outObjectID;

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_TextureIndex;
layout(location = 2) flat in int  i_EntityID;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) float g_Gamma;
};

void main()
{
    vec4 color = ReadTexture(i_TextureIndex, i_TexCoords);
    color.rgb = pow(color.rgb, vec3(g_Gamma));

    outAlbedo = color;
    outObjectID = i_EntityID;
}
