#extension GL_EXT_nonuniform_qualifier : enable

#include "defines.h"
#include "utils.h"
#include "text/msdf.h"

#ifdef EG_MATERIALS_REQUIRED
#include "pipeline_layout.h"
const uint s_Set = 3;
#else
const uint s_Set = 1;
#endif

layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_AtlasIndex;
#ifdef EG_MATERIALS_REQUIRED
layout(location = 2) flat in uint i_MaterialIndex;
layout(location = 3) in vec2 i_MaterialUV;
#endif

#ifdef EG_TRANSLUCENT
layout(location = 0) out vec4 o_Color;
#ifdef EG_OUTPUT_DEPTH
layout(location = 1) out float o_Depth;
#endif
#endif

layout(set = s_Set, binding = 0) uniform sampler2D g_FontAtlases[];

void main()
{
#ifdef EG_MATERIALS_REQUIRED
    vec2 uv = i_MaterialUV;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
#endif

#ifdef EG_MASKED
	if (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD)
	{
		discard;
		return;
	}
#endif

	const float opacity = MSDF_COVERAGE(g_FontAtlases[nonuniformEXT(i_AtlasIndex)], i_TexCoords);

	if (opacity < 0.5f)
		discard;

#ifdef EG_TRANSLUCENT
	o_Color = vec4(material.Albedo * (1.f - material.Opacity), 1.f);
#ifdef EG_OUTPUT_DEPTH
	o_Depth = gl_FragCoord.z;
#endif
#endif
}
