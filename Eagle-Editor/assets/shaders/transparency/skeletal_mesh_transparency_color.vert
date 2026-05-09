#include "defines.h"
#include "skeletal_mesh_vertex_input_layout.h"
#define EG_NO_TEXTURES
#include "pipeline_layout.h"

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
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(set = 5, binding = 1)
uniform CameraMatrices
{
    mat4 g_View;
    mat4 g_InvViewProj;
    mat4 g_ViewProjection;
    mat4 g_PrevViewProjection;
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) out vec3 o_WorldPos;
layout(location = 4) out mat3 o_TBN;

void main()
{
    const InstanceData instanceData = g_InstanceData[gl_InstanceIndex];
    const uint vertexIndex = GetVertexOffset(instanceData) + gl_VertexIndex;
    const Vertex vertex = g_SkinnedVertices[vertexIndex];

    const uint transformIndex = GetTransformIndex(instanceData);
    gl_Position = g_ViewProjection * vec4(vertex.Position, 1.0);
    
    const mat4 model = g_Transforms[transformIndex];
    const vec3 normal = vertex.Normal;
    const mat3 normalModel = transpose(inverse(mat3(model)));
    const vec3 worldNormal = normalize(normalModel * normal);

    const uint materialIndex = GetMaterialIndex(instanceData);
    const uint normalTextureIndex = FetchMaterialNormalTextureIndex(materialIndex);
    if (normalTextureIndex != EG_INVALID_INDEX)
    {
        vec3 tangent = normalize(normalModel * vertex.Tangent);
        tangent = normalize(tangent - worldNormal * dot(tangent, worldNormal));
        vec3 bitangent = normalize(cross(worldNormal, tangent));
        o_TBN = mat3(tangent, bitangent, worldNormal);
    }

    o_WorldPos = vertex.Position;
    o_Normal = worldNormal;
    o_TexCoords = vertex.TexCoords;
    o_MaterialIndex = materialIndex;
}
