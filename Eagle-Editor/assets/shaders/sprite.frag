#include "pipeline_layout.h"
#include "utils.h"

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_PackedTextureIndices;

void main()
{
    uint diffuseTextureIndex, specularTextureIndex, normalTextureIndex;
	UnpackTextureIndices(i_PackedTextureIndices, diffuseTextureIndex, specularTextureIndex, normalTextureIndex);

    vec3 normal;
	if (normalTextureIndex != INVALID_TEXTURE_INDEX)
	{
		normal = ReadTexture(normalTextureIndex, i_TexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(i_TBN * normal);
	}
	else
	{
	    normal = normalize(i_Normal);
	}

    outAlbedo = ReadTexture(diffuseTextureIndex, i_TexCoords);
	outNormal = vec4(EncodeNormal(normal), 1.f);
}
