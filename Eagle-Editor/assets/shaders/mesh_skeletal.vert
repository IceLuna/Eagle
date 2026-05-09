#extension GL_EXT_nonuniform_qualifier : enable

#include "skeletal_mesh_vertex_input_layout.h"

#ifndef EG_DEPTH_ONLY
#define EG_NO_TEXTURES
#include "pipeline_layout.h"
#endif

layout(scalar, set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer SkinnedVertices
{
    Vertex g_SkinnedVertices[];
};

layout(scalar, set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 1)
readonly buffer PerInstanceDataBuffer
{
    InstanceData g_InstanceData[];
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 2)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

#ifndef EG_DEPTH_ONLY
layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 3)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};
#endif

#ifdef EG_MOTION
layout(scalar, set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 4)
readonly buffer PrevSkinnedVerticesPositions
{
    vec3 g_PrevSkinnedVertexPosition[];
};
#endif

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
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];
    
    const uint vertexIndex = GetVertexOffset(instanceData) + gl_VertexIndex;
    const Vertex vertex = g_SkinnedVertices[vertexIndex];

    const uint transformIndex = GetTransformIndex(instanceData);
    gl_Position = g_ViewProjection * vec4(vertex.Position, 1.0);

#ifndef EG_DEPTH_ONLY
    const mat4 model = g_Transforms[transformIndex];
    const uint materialIndex = GetMaterialIndex(instanceData);
    const uint objectID = GetObjectID(instanceData);
    o_ReceivesDecals = DoesReceiveDecals(instanceData) ? 1u : 0u;

    const vec3 normal = vertex.Normal;
    const mat3 normalModel = transpose(inverse(mat3(model)));
    const vec3 worldNormal = normalize(normalModel * normal);

    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(normalModel * vertex.Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_Normal = worldNormal;
    o_TexCoords = vertex.TexCoords;
    o_MaterialIndex = materialIndex;
    o_ObjectID = objectID;
#endif // #ifndef EG_DEPTH_ONLY

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;
    {
        const vec3 prevVertexPos = g_PrevSkinnedVertexPosition[vertexIndex];
        const vec4 prevPos = g_PrevViewProjection * vec4(prevVertexPos, 1.0);
        o_PrevPos = prevPos.xyw;
    }
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
