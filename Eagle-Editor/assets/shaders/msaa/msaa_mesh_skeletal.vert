#extension GL_EXT_nonuniform_qualifier : enable

#include "skeletal_mesh_vertex_input_layout.h"

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

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX + 3)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
#ifdef EG_MASKED
layout(location = 2) flat out uint o_MaterialIndex;
#endif

void main()
{
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];
    
    const uint vertexIndex = GetVertexOffset(instanceData) + gl_VertexIndex;
    const Vertex vertex = g_SkinnedVertices[vertexIndex];

    gl_Position = g_ViewProjection * vec4(vertex.Position, 1.0);

    const uint transformIndex = GetTransformIndex(instanceData);
    const mat4 model = g_Transforms[transformIndex];
    const vec3 normal = vertex.Normal;
    const mat3 normalModel = transpose(inverse(mat3(model)));
    const vec3 worldNormal = normalize(normalModel * normal);

    o_Normal = worldNormal;
    o_TexCoords = vertex.TexCoords;
#ifdef EG_MASKED
    o_MaterialIndex = GetMaterialIndex(instanceData);
#endif
}
