#include "pipeline_layout.h"
#include "mesh_material_pipeline_layout.h"
#include "utils.h"

// Input
layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_MaterialIndex;
layout(location = 6) flat in uint i_ObjectID;

// Output
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryNormal;
layout(location = 2) out vec4 outShadingNormal;
layout(location = 3) out vec4 outEmissive;
layout(location = 4) out vec4 outMaterialData;
layout(location = 5) out int outObjectID;

void main()
{
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
	const vec2 uv = i_TexCoords * material.TilingFactor;

    const vec3 geometryNormal = normalize(i_Normal);
    vec3 shadingNormal = geometryNormal;
	if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		shadingNormal = ReadTexture(material.NormalTextureIndex, uv).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
	}

	const float metallness = ReadTexture(material.MetallnessTextureIndex, uv).x;
	float roughness = (material.RoughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.RoughnessTextureIndex, uv).x : EG_DEFAULT_ROUGHNESS;
	roughness = max(roughness, EG_MIN_ROUGHNESS);
	const float ao = (material.AOTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.AOTextureIndex, uv).r : EG_DEFAULT_AO;

	// TODO: optimize better? Normals.a & materialData.a & emission.a are unused (!)
    outAlbedo = ReadTexture(material.AlbedoTextureIndex, uv) * material.TintColor;
    outGeometryNormal = vec4(EncodeNormal(geometryNormal), 1.f);
    outShadingNormal = vec4(EncodeNormal(shadingNormal), 1.f);
	outEmissive = ReadTexture(material.EmissiveTextureIndex, uv) * vec4(material.EmissiveIntensity, 1.f);
	outMaterialData = vec4(metallness, roughness, ao, 1.f);
	outObjectID = int(i_ObjectID);
}
