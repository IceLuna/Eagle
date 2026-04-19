#include "defines.h"
#include "common_structures.h"
#include "pipeline_layout.h"
#include "utils.h"

layout(location = 0) in vec4 i_ClipPos;
layout(location = 1) flat in vec2 i_AspectRatio;
layout(location = 2) flat in uint i_MaterialIndex;
layout(location = 3) flat in uint i_TransformIndex;
layout(location = 4) flat in uint i_EntityID;

// Outputs
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outMaterialData;
layout(location = 3) out int  outObjectID;
#ifdef DECAL_NORMALS
layout(location = 4) out vec4 outGeometryShadingNormals;
#endif

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX) readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1) uniform sampler2D g_Depth;
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 2) uniform usampler2D g_Flags;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) mat4 g_InvVP;
};

vec2 ComputeUV(vec4 localPos)
{
	vec2 uv = localPos.xy * 0.5 + 0.5;
	uv.y = 1 - uv.y;
	return uv;
}

mat3 ComputeTBN(vec3 worldPos, vec2 uv, out vec3 normal)
{
    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);

    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 T = dp1 * duv2.y - dp2 * duv1.y;
    vec3 B = dp2 * duv1.x - dp1 * duv2.x;

    float invMax = inversesqrt(max(dot(T,T), dot(B,B)));

    T *= invMax;
    B *= invMax;

    normal = -normalize(cross(T, B));

    return mat3(T, B, normal);
}

void main()
{
	vec2 uv = (i_ClipPos.xy / i_ClipPos.w) * 0.5f + 0.5f;
	const float depth = texture(g_Depth, uv).x;
    if (depth == EG_DEPTH_FAR)
		discard;

    const vec3 worldPos = WorldPosFromDepth(g_InvVP, uv, depth);

	const mat4 decalInvTr = g_Transforms[i_TransformIndex];
    vec4 localPos = decalInvTr * vec4(worldPos, 1.0);
	localPos.xy *= i_AspectRatio;

    if (abs(localPos.x) > 1.0 ||
		abs(localPos.y) > 1.0 ||
		abs(localPos.z) > 1.f)
	{
        discard;
	}

	const uint flags = texture(g_Flags, uv).x;
	const bool bReceivesDecals = (flags & EG_FLAGS_RECEIVES_DECALS_MASK) == EG_FLAGS_RECEIVES_DECALS_MASK;
	if (!bReceivesDecals)
		discard;

	vec2 decalUV = ComputeUV(localPos);
	const ShaderMaterial material = FetchMaterial(i_MaterialIndex, decalUV);
	if (material.OpacityMask < EG_OPACITY_MASK_THRESHOLD || IS_ZERO(material.Opacity))
		discard;

	const float metalness = material.Metalness;
	const float roughness = material.Roughness;
	const float ao = material.AO;

	outAlbedo = vec4(material.Albedo, material.Opacity);
	outEmissive = vec4(material.Emissive, material.Opacity); // Blending based on opacity
	outMaterialData = vec4(metalness, ao, roughness, material.Opacity); // Blending based on opacity
	outObjectID = int(i_EntityID);

#ifdef DECAL_NORMALS
	{
		vec3 shadingNormal = ReadTexture(material.NormalTextureIndex, decalUV).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);

		vec3 geometryNormal;
		shadingNormal = normalize(ComputeTBN(worldPos, decalUV, geometryNormal) * shadingNormal);

		outGeometryShadingNormals = vec4(EncodeNormal(geometryNormal), EncodeNormal(shadingNormal));
	}
#endif
}
