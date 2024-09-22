#include "pipeline_layout.h"
#include "utils.h"

// Outputs
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outGeometryShadingNormals;
layout(location = 2) out vec4 outEmissive;
layout(location = 3) out vec4 outMaterialData;
layout(location = 4) out float outFlags;
layout(location = 5) out int  outObjectID;
#ifdef EG_MOTION
layout(location = 6) out vec2 outMotion;
#endif

// Inputs
layout(location = 0) in mat3 i_TBN;
layout(location = 3) in vec3 i_Normal;
layout(location = 4) in vec2 i_TexCoords; // Tiled
layout(location = 5) flat in uint i_MaterialIndex;
layout(location = 6) flat in int  i_EntityID;
layout(location = 7) flat in uint i_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 8) in vec3 i_CurPos;
layout(location = 9) in vec3 i_PrevPos;
#endif

void main()
{
	vec2 uv = i_TexCoords;
	const ShaderMaterial material = FetchMaterial(i_MaterialIndex, uv);
#ifdef EG_MASKED
	if (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD)
	{
		discard;
		return;
	}
#endif

    const vec2 packedGeometryNormal = EncodeNormal(normalize(i_Normal));
	vec2 packedShadingNormal = packedGeometryNormal;
	if (material.NormalTextureIndex != EG_INVALID_INDEX)
	{
		vec3 shadingNormal = ReadTexture(material.NormalTextureIndex, uv).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
		shadingNormal = normalize(i_TBN * shadingNormal);
		packedShadingNormal = EncodeNormal(shadingNormal);
	}

	const float metalness = material.Metalness;
	const float roughness = material.Roughness;
	const float ao = material.AO;
	
	outAlbedo = vec4(material.Albedo * material.TintColor.rgb, 1.f);
	outGeometryShadingNormals = vec4(packedGeometryNormal, packedShadingNormal);
	outEmissive = vec4(material.Emissive * material.EmissiveIntensity, 1.0);
	outMaterialData = vec4(metalness, ao, roughness, 0);
	outObjectID = i_EntityID;
	outFlags = i_ReceivesDecals == 1u ? 1.f : 0.f;

#ifdef EG_MOTION
	outMotion = ((i_CurPos.xy / i_CurPos.z) - (i_PrevPos.xy / i_PrevPos.z)) * 0.5f; // The + 0.5 part is unnecessary, since it cancels out in a-b anyway
#endif
}
