#include "pipeline_layout.h"
#include "material_pipeline_layout.h"
#include "utils.h"

// Outputs
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryShadingNormals;
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out vec2 outMaterialData;
layout(location = 4) out int  outObjectID;
#ifdef EG_MOTION
layout(location = 5) out vec2 outMotion;
#endif

// Inputs
layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords; // Tiled
layout(location = 5) flat in uint i_MaterialIndex;
layout(location = 6) flat in int  i_EntityID;
#ifdef EG_MOTION
layout(location = 7) in vec3 i_CurPos;
layout(location = 8) in vec3 i_PrevPos;
#endif

void main()
{
	const ShaderMaterial material = FetchMaterial(i_MaterialIndex);
#ifdef EG_MASKED
	if (material.OpacityMaskTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		const float opacityMask = ReadTexture(material.OpacityMaskTextureIndex, i_TexCoords).r;
		if (opacityMask < EG_OPACITY_MASK_THRESHOLD)
		{
			discard;
			return;
		}
	}
#endif

    const vec2 packedGeometryNormal = EncodeNormal(normalize(i_Normal));
	vec2 packedShadingNormal = packedGeometryNormal;
	if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
	{
		vec3 shadingNormal = ReadTexture(material.NormalTextureIndex, i_TexCoords).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
		packedShadingNormal = EncodeNormal(shadingNormal);
	}

	const float metallness = ReadTexture(material.MetallnessTextureIndex, i_TexCoords).x;
	float roughness = (material.RoughnessTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.RoughnessTextureIndex, i_TexCoords).x : EG_DEFAULT_ROUGHNESS;
	roughness = max(roughness, EG_MIN_ROUGHNESS);
	const float ao = (material.AOTextureIndex != EG_INVALID_TEXTURE_INDEX) ? ReadTexture(material.AOTextureIndex, i_TexCoords).r : EG_DEFAULT_AO;

    outAlbedo = vec4(ReadTexture(material.AlbedoTextureIndex, i_TexCoords).rgb * material.TintColor.rgb, roughness);
	outGeometryShadingNormals = vec4(packedGeometryNormal, packedShadingNormal);
	outEmissive = ReadTexture(material.EmissiveTextureIndex, i_TexCoords) * vec4(material.EmissiveIntensity, 1.0);
	outMaterialData = vec2(metallness, ao);
	outObjectID = i_EntityID;

	// TODO: Pack to outEmissive.a since it's not used anyway
#ifdef EG_MOTION
	outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
