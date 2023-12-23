#include "pipeline_layout.h"
#include "material_pipeline_layout.h"

// Input
layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_MaterialIndex;

void main()
{
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
	if (material.OpacityMaskTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		const vec2 uv = i_TexCoords * material.TilingFactor;
		const float opacityMask = ReadTexture(material.OpacityMaskTextureIndex, uv).r;
		if (opacityMask < EG_OPACITY_MASK_THRESHOLD)
		{
			discard;
			return;
		}
	}
}
