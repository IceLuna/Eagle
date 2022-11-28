#include "pipeline_layout.h"
#include "utils.h"

// Input
layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_MaterialIndex;

// Output
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

void main()
{
    ShaderMaterial material = FetchMaterial(i_MaterialIndex);

    vec3 normal;
	if (material.NormalTextureIndex != INVALID_TEXTURE_INDEX)
	{
		normal = ReadTexture(material.NormalTextureIndex, i_TexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(i_TBN * normal);
	}
	else
	{
	    normal = normalize(i_Normal);
	}

    outAlbedo = ReadTexture(material.DiffuseTextureIndex, i_TexCoords);
    outNormal = vec4(EncodeNormal(normal), 1.f);
}
