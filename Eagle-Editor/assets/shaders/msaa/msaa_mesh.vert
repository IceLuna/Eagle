#include "mesh_vertex_input_layout.h"

layout(set = EG_PERSISTENT_SET, binding = EG_BINDING_MAX)
readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProjection;
};

#ifdef EG_JITTER
layout(set = 1, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

layout(location = 0) out vec3 o_Normal;
layout(location = 1) out vec2 o_TexCoords;
#ifdef EG_MASKED
layout(location = 2) flat out uint o_MaterialIndex;
#endif

void main()
{
    const uint transformIndex = GetTransformIndex();
    const mat4 model = g_Transforms[transformIndex];
    gl_Position = g_ViewProjection * model * vec4(a_Position, 1.0);

    const mat3 normalModel = mat3(transpose(inverse(model)));
    const vec3 worldNormal = normalize(normalModel * a_Normal);

    o_Normal = worldNormal;
    o_TexCoords = a_TexCoords;
#ifdef EG_MASKED
    o_MaterialIndex = GetMaterialIndex();
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif
}
