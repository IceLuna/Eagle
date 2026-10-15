#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"
#include "text/msdf.h"

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_AtlasIndex;
#ifdef EG_MASKED
layout(location = 2) flat in float i_OpacityMask;
#endif
#ifdef EG_TRANSLUCENT
layout(location = 2) flat in vec3  i_Albedo;
layout(location = 3) flat in float i_Opacity;

layout(location = 0) out vec4 o_Color;
#ifdef EG_OUTPUT_DEPTH
layout(location = 1) out float o_Depth;
#endif
#endif

layout(set = 1, binding = 0) uniform sampler2D g_FontAtlases[];

void main()
{
#ifdef EG_MASKED
	if (i_OpacityMask < EG_OPACITY_MASK_THRESHOLD)
	{
		discard;
		return;
	}
#endif

	const float opacity = MSDF_COVERAGE(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);

	if (opacity < 0.5f)
		discard;

#ifdef EG_TRANSLUCENT
	o_Color = vec4(i_Albedo * (1.f - i_Opacity), 1.f);
#ifdef EG_OUTPUT_DEPTH
	o_Depth = gl_FragCoord.z;
#endif
#endif
}
