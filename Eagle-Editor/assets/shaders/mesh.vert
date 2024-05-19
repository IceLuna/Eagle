#include "pipeline_layout.h"
#include "mesh_vertex_input_layout.h"
#include "material_pipeline_layout.h"

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

layout(location = 0) out mat3 o_TBN;
layout(location = 3) out vec3 o_Normal;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_MaterialIndex;
layout(location = 6) flat out uint o_ObjectID;
#ifdef EG_MOTION
layout(location = 7) out vec3 o_CurPos;
layout(location = 8) out vec3 o_PrevPos;
#endif

void main()
{
    const mat4 model = g_Transforms[a_PerInstanceData.x];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);
    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * a_Normal);

    ShaderMaterial material = FetchMaterial(a_PerInstanceData.y);
    if (material.NormalTextureIndex != EG_INVALID_TEXTURE_INDEX)
    {
        vec3 tangent = normalize(normalModel * a_Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = a_PerInstanceData.y;
    o_ObjectID = a_PerInstanceData.z;

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[a_PerInstanceData.x];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(a_Position, 1.0);
    o_PrevPos = prevPos.xyw;
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
