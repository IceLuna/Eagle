#include "pipeline_layout.h"
#include "material_pipeline_layout.h"
#include "utils.h"

// Input
layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords;
layout(location = 5) flat in uint i_MaterialIndex;
layout(location = 6) flat in uint i_ObjectID;
#ifdef EG_MOTION
layout(location = 7) in vec3 i_CurPos;
layout(location = 8) in vec3 i_PrevPos;
#endif

// Output
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryShadingNormals;
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out vec2 outMaterialData;
layout(location = 4) out int outObjectID;
#ifdef EG_MOTION
layout(location = 5) out vec2 outMotion;
#endif

void main()
{
    const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
	const vec2 uv = i_TexCoords * material.TilingFactor;

    const vec2 packedGeometryNormal = EncodeNormal(normalize(i_Normal));
	vec2 packedShadingNormal = packedGeometryNormal;
	if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		vec3 shadingNormal = ReadTexture(material.NormalTextureIndex, uv).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
		packedShadingNormal = EncodeNormal(shadingNormal);
	}

	const float metallness = ReadTexture(material.MetallnessTextureIndex, uv).x;
	float roughness = (material.RoughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.RoughnessTextureIndex, uv).x : EG_DEFAULT_ROUGHNESS;
	roughness = max(roughness, EG_MIN_ROUGHNESS);
	const float ao = (material.AOTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.AOTextureIndex, uv).r : EG_DEFAULT_AO;

	// TODO: optimize better? emission.a is unused (!)
    outAlbedo = vec4(ReadTexture(material.AlbedoTextureIndex, uv).rgb * material.TintColor.rgb, roughness);
    outGeometryShadingNormals = vec4(packedGeometryNormal, packedShadingNormal);
	outEmissive = ReadTexture(material.EmissiveTextureIndex, uv) * vec4(material.EmissiveIntensity, 1.f);
	outMaterialData = vec2(metallness, ao);
	outObjectID = int(i_ObjectID);

	// TODO: Pack to outEmissive.a since it's not used anyway
#ifdef EG_MOTION
	outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
