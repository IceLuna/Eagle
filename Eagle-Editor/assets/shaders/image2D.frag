#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"

layout(location = 0) in vec4 i_Tint_Opacity;
layout(location = 1) in vec2 i_TexCoords;
layout(location = 2) flat in uint i_TextureIndex;
#ifndef EG_NO_OBJECT_ID
layout(location = 3) flat in int i_EntityID;
#endif

layout(location = 0) out vec4 outColor;
#ifndef EG_NO_OBJECT_ID
layout(location = 1) out int  outObjectID;
#endif

layout(binding = 0) uniform sampler2D g_Textures[EG_MAX_TEXTURES];

void main()
{
	vec4 color = texture(g_Textures[nonuniformEXT(i_TextureIndex)], i_TexCoords);
	color *= i_Tint_Opacity;

	if (IS_ZERO(color.a))
		discard;

    outColor = color;
#ifndef EG_NO_OBJECT_ID
    outObjectID = i_EntityID;
#endif
}
