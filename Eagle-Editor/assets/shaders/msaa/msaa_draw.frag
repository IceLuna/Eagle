#include "pipeline_layout.h"
#include "utils.h"

// Input
layout(location = 0) in vec3 i_Normal;
layout(location = 1) in vec2 i_TexCoords;
#ifdef EG_MASKED
layout(location = 2) flat in uint i_MaterialIndex;
#endif

// Output
layout(location = 0) out vec4 outGeometryNormals;

void main()
{
#ifdef EG_MASKED
	vec2 uv = i_TexCoords;
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
#endif

#ifdef EG_BACKFACE_FLIP_NORMAL
	const vec3 geomNormal = gl_FrontFacing ? i_Normal : -i_Normal;
#else
	const vec3 geomNormal = i_Normal;
#endif
    const vec2 packedGeometryNormal = EncodeNormal(normalize(geomNormal));

#ifdef EG_MASKED
    outGeometryNormals = vec4(packedGeometryNormal, 0, material.OpacityMask < EG_OPACITY_MASK_THRESHOLD ? material.OpacityMask : 1);
#else
    outGeometryNormals = vec4(packedGeometryNormal, 0, 1);
#endif
}
