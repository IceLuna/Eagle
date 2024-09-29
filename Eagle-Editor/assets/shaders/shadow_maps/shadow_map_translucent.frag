#include "pipeline_layout.h"

// Input
layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_MaterialIndex;

layout(location = 0) out vec4 o_Color;
#ifdef EG_OUTPUT_DEPTH
layout(location = 1) out float o_Depth;
#endif

void main()
{
    vec2 uv = i_TexCoords;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);

    const vec3 color = material.Albedo;
    o_Color = vec4(color * (1.f - material.Opacity), 1.f);

#ifdef EG_OUTPUT_DEPTH
    o_Depth = gl_FragCoord.z;
#endif
}
