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

// Converts the opacity mask into pixel coverage instead of doing a hard alpha test.
// A hard `mask < threshold -> discard` breaks down when the mask is minified (far away / grazing angles):
// Mips average the mask, so a thin opaque line (mask = 1) turns into a wide faint one (e.g. mask = 0.2). That's below the threshold, so the line disappears.
// Around the threshold the result depends on sub-pixel position, so it flickers as the camera moves.
// Decals are alpha blended anyway, so coverage can be used directly as blend weight.
float ComputeMaskCoverage(float mask, vec2 opacityMaskTextureSize, vec2 uv)
{
    // Anti-aliased alpha test: turns the cutoff into a ~1 pixel wide gradient instead of a step.
    // Keeps edges crisp when the texture is magnified.
    const float sharpCoverage = clamp((mask - EG_OPACITY_MASK_THRESHOLD) / max(fwidth(mask), 1e-4) + 0.5, 0.0, 1.0);

    const vec2 texelUV = uv * opacityMaskTextureSize;
    const vec2 dx = dFdx(texelUV);
    const vec2 dy = dFdy(texelUV);
    const float lod = 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-8));

    const float filteredCoverage = clamp(mask / EG_OPACITY_MASK_THRESHOLD, 0.0, 1.0);
    const float coverage = mix(sharpCoverage, filteredCoverage, clamp(lod, 0.0, 1.0));

    return opacityMaskTextureSize.x > 0.0 ? coverage : step(EG_OPACITY_MASK_THRESHOLD, mask);
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

    const vec3 worldPos = WorldPosFromDepth(g_InvVP, uv, depth);

	const mat4 decalInvTr = g_Transforms[i_TransformIndex];
    vec4 localPos = decalInvTr * vec4(worldPos, 1.0);
	localPos.xy *= i_AspectRatio;

	const uint flags = texture(g_Flags, uv).x;
	const bool bReceivesDecals = (flags & EG_FLAGS_RECEIVES_DECALS_MASK) == EG_FLAGS_RECEIVES_DECALS_MASK;

	const bool bInsideDecal = depth != EG_DEPTH_FAR &&
		abs(localPos.x) <= 1.0 &&
		abs(localPos.y) <= 1.0 &&
		abs(localPos.z) <= 1.0 &&
		bReceivesDecals;

	// Note: there are no early discards on purpose. Texture sampling (mip selection), fwidth() and ComputeTBN()
	// use derivatives, which are computed across 2x2 pixel quads. Derivatives are undefined in a quad
	// where some pixels were already discarded, which breaks mips and coverage along decal/object edges.
	// So everything is computed first, and pixels are discarded at the very end.
	vec2 decalUV = ComputeUV(localPos);
	const ShaderMaterial material = FetchMaterial(i_MaterialIndex, decalUV);

	const uint opacityMaskTextureIndex = FetchMaterialOpacityMaskTextureIndex(i_MaterialIndex);
	const bool bValidOpacityMaskTexture = (material.BlendMode == BlendMode_Masked) && (opacityMaskTextureIndex != EG_INVALID_INDEX);
	const vec2 opacityMaskTextureSize = bValidOpacityMaskTexture ? ReadTextureSize(opacityMaskTextureIndex) : vec2(0);

	const float coverage = ComputeMaskCoverage(material.OpacityMask, opacityMaskTextureSize, decalUV);
	const float opacity = material.Opacity * coverage;

#ifdef DECAL_NORMALS
	vec3 geometryNormal;
	{
		vec3 shadingNormal = ReadTexture(material.NormalTextureIndex, decalUV).rgb;
		shadingNormal = normalize(shadingNormal * 2.0 - 1.0);

		shadingNormal = normalize(ComputeTBN(worldPos, decalUV, geometryNormal) * shadingNormal);

		outGeometryShadingNormals = vec4(EncodeNormal(geometryNormal), EncodeNormal(shadingNormal));
	}
	const float roughness = ApplyGeometricSpecularAntiAliasing(geometryNormal, material.Roughness);

	// The normals target isn't blended, every surviving pixel overwrites it completely.
	// So normals can't fade with coverage and use a 50% cutoff instead (the other targets still get soft coverage)
	const bool bVisible = coverage >= 0.5 && NOT_ZERO(material.Opacity);
#else
	const float roughness = material.Roughness;
	const bool bVisible = opacity > (0.5 / 255.0);
#endif

	if (!bInsideDecal || !bVisible)
		discard;

	const float metalness = material.Metalness;
	const float ao = material.AO;

	outAlbedo = vec4(material.Albedo, opacity);
	outEmissive = vec4(material.Emissive, opacity); // Blending based on opacity
	outMaterialData = vec4(metalness, ao, roughness, opacity); // Blending based on opacity
	outObjectID = int(i_EntityID);
}
