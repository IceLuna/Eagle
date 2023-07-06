#include "defines.h"
#include "sprite_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
layout(location = 3) flat out int o_EntityID;

void main()
{
    const uint materialIndex = gl_VertexIndex / 8u;
    const uint transformIndex = gl_VertexIndex / 8u;

    const mat4 model = g_Transforms[transformIndex];

    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) >= 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    o_TexCoords = a_TexCoords;
    o_MaterialIndex  = materialIndex;
    o_EntityID = a_EntityID;
}
