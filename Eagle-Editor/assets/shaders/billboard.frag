#include "pipeline_layout.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out int  outObjectID;
#ifdef EG_MOTION
layout(location = 2) out vec2 outMotion;
#endif

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_TextureIndex;
layout(location = 2) flat in int  i_EntityID;
#ifdef EG_MOTION
layout(location = 3) in vec3 i_CurPos;
layout(location = 4) in vec3 i_PrevPos;
#endif

void main()
{
    const vec4 color = ReadTexture(i_TextureIndex, i_TexCoords);

    // Alpha mask
    if (color.a < 0.9f)
        discard;

    outAlbedo = color;
    outObjectID = i_EntityID;

#ifdef EG_MOTION
	outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
