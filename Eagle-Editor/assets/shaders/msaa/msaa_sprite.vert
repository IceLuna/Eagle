#include "defines.h"
#include "sprite_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
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
    const uint transformIndex = a_TransformIndex & (~EG_RECEIVES_DECALS_MASK); // Get all but the highest bit
    const mat4 model = g_Transforms[transformIndex];
    const uint vertexID = gl_VertexIndex % 4u;
    gl_Position = g_ViewProj * model * vec4(s_QuadVertexPosition[vertexID], 1.f);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * s_Normal);

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
#ifdef EG_MASKED
    o_MaterialIndex  = a_MaterialIndex;
#endif
}
