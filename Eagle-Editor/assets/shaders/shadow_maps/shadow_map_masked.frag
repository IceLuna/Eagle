#include "pipeline_layout.h"

// Input
layout(location = 0) in vec2 i_TexCoords;
layout(location = 1) flat in uint i_MaterialIndex;

void main()
{
	vec2 uv = i_TexCoords;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
	if (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD)
	{
		discard;
		return;
	}
}
