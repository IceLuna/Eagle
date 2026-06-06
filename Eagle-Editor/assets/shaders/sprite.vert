#include "defines.h"
#include "sprite_vertex_input_layout.h"

#ifndef EG_DEPTH_ONLY
#define EG_NO_TEXTURES
#include "pipeline_layout.h"
#endif

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1)
readonly buffer MeshPrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};
#endif

layout(set = 1, binding = 0) uniform CameraData
{
	mat4 g_View;
	mat4 g_InvViewProj;
	mat4 g_ViewProj;
	mat4 g_PrevViewProj;
	mat4 g_Proj;
	mat4 g_InvProj;
	mat4 g_PrevProj;
	mat4 g_PrevView;
	mat4 g_ViewProjUnjittered;
	mat4 g_PrevViewProjUnjittered;
};

#ifndef EG_DEPTH_ONLY

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out int o_EntityID;
layout(location = 7) flat out uint o_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

#endif // #ifndef EG_DEPTH_ONLY

void main()
{
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);

#ifndef EG_DEPTH_ONLY
    const uint materialIndex  = a_MaterialIndex;
    o_ReceivesDecals = (a_TransformIndex & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK ? 1u : 0u;

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * s_Normal);

    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        const vec3 worldTangent = normalize(normalModel * s_Tangent);
        const vec3 worldBitangent = normalize(normalModel * s_Bitangent);
        o_TBN = mat3(worldTangent, worldBitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex  = materialIndex;
    o_EntityID = a_EntityID;
#endif // #ifndef EG_DEPTH_ONLY

#ifdef EG_MOTION
    const vec4 curPos = g_ViewProjUnjittered * model * vec4(s_QuadVertexPosition[vertexID], 1.0);
    o_CurPos = curPos.xyw;

    const mat4 prevModel = g_PrevTransforms[transformIndex];
    const vec4 prevPos = g_PrevViewProjUnjittered * prevModel * vec4(s_QuadVertexPosition[vertexID], 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
