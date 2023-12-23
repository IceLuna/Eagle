#include "pipeline_layout.h"
#include "material_pipeline_layout.h"

// Input
layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_MaterialIndex;

layout(location = 0) out vec4 o_Color;
#ifdef EG_OUTPUT_DEPTH
layout(location = 1) out float o_Depth;
#endif

void main()
{
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
    const vec2 uv = i_TexCoords * material.TilingFactor;

    float opacity = material.OpacityTextureIndex != EG_INVALID_TEXTURE_INDEX ? ReadTexture(material.OpacityTextureIndex, uv).r : 0.5f;
    opacity = clamp(opacity * material.TintColor.a, 0.f, 1.f);

    const vec3 color = ReadTexture(material.AlbedoTextureIndex, uv).rgb;
    o_Color = vec4(color * (1.f - opacity), 1.f);

#ifdef EG_OUTPUT_DEPTH
    o_Depth = gl_FragCoord.z;
#endif
}
