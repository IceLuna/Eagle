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

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX) readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

layout(set = 3, binding = 0) uniform sampler2D g_Depth;
layout(set = 3, binding = 1) uniform sampler2D g_Flags;

layout(push_constant) uniform PushConstants
{
    layout(offset = 64) mat4 g_InvVP;
};

void main()
{
	vec2 uv = (i_ClipPos.xy / i_ClipPos.w) * 0.5f + 0.5f;
	const float depth = texture(g_Depth, uv).x;
    const vec3 worldPos = WorldPosFromDepth(g_InvVP, uv, depth);

	const mat4 decalVP = g_Transforms[i_TransformIndex];
    vec4 ndcPos = decalVP * vec4(worldPos, 1.0);
    ndcPos.xyz /= ndcPos.w;
	ndcPos.xy *= i_AspectRatio;

    if (ndcPos.x < -1.0 || ndcPos.x > 1.0 ||
		ndcPos.y < -1.0 || ndcPos.y > 1.0 ||
		ndcPos.z < -1.0 || ndcPos.z > 1.f)
	{
        discard;
	}

	const float receivesDecal = texture(g_Flags, uv).x; // 0.0 if doesn't receive decals
	if (receivesDecal < 0.1)
		discard;

	vec2 decalUV = ndcPos.xy * 0.5f + 0.5f;
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
}
