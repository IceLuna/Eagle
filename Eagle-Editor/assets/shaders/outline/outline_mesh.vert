#include "mesh_vertex_input_layout.h"
#include "outline/outline_common.h"

layout(set = EG_OUTLINE_GEOMETRY_SET, binding = 0) readonly buffer MeshTransformsBuffer
{
    mat4 g_Transforms[];
};

layout(location = 0) flat out uint o_StyleID;
#ifdef EG_MATERIALS_REQUIRED
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out uint o_MaterialIndex;
#endif

void main()
{
    const mat4 model = g_Transforms[GetTransformIndex()];
    gl_Position = g_PushData.ViewProj * model * vec4(a_Position, 1.0);

    o_StyleID = g_PushData.StyleID;
#ifdef EG_MATERIALS_REQUIRED
    o_TexCoords = a_TexCoords;
    o_MaterialIndex = GetMaterialIndex();
#endif
}
