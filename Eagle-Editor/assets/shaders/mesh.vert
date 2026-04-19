#include "mesh_vertex_input_layout.h"
#include "defines.h"

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

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
};

#ifdef EG_JITTER
layout(set = 1, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

#ifndef EG_DEPTH_ONLY

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out uint o_ObjectID;
layout(location = 7) flat out uint o_ReceivesDecals;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

#endif // #ifndef EG_DEPTH_ONLY

void main()
{
    const uint transformIndex = a_PerInstanceData.x & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);

#ifndef EG_DEPTH_ONLY
    const uint materialIndex = a_PerInstanceData.y;
    const uint objectID = a_PerInstanceData.z;
    o_ReceivesDecals = (a_PerInstanceData.x & EG_RECEIVES_DECALS_MASK) == EG_RECEIVES_DECALS_MASK ? 1u : 0u;

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * a_Normal);

    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(normalModel * a_Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = materialIndex;
    o_ObjectID = objectID;
#endif // #ifndef EG_DEPTH_ONLY

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[transformIndex];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(a_Position, 1.0);
    o_PrevPos = prevPos.xyw;
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
