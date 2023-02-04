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
layout(location = 2) out vec4 outMaterialData;

void main()
{
    ShaderMaterial material = FetchMaterial(i_MaterialIndex);

    vec3 normal;
	if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		normal = ReadTexture(material.NormalTextureIndex, i_TexCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		normal = normalize(i_TBN * normal);
	}
	else
	{
	    normal = normalize(i_Normal);
	}

	const float metallness = ReadTexture(material.MetallnessTextureIndex, i_TexCoords).x;
	float roughness = (material.RoughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.RoughnessTextureIndex, i_TexCoords).x : 0.5f; // Default roughness = 0.5f
	roughness = max(roughness, 0.04f);
	float ao = (material.AOTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.AOTextureIndex, i_TexCoords).r : 1.f; // default ao = 1.f

    outAlbedo = ReadTexture(material.AlbedoTextureIndex, i_TexCoords);
    outNormal = vec4(EncodeNormal(normal), 1.f);
	outMaterialData = vec4(metallness, roughness, ao, 1.f);
}
