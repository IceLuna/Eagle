#include "text_lit_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
#ifdef EG_MOTION
    mat4 g_PrevViewProjection;
#endif
};

layout(binding = 0)
buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(binding = 1)
buffer PrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};
#endif

layout(location = 0) flat out vec4 o_AlbedoRoughness;
layout(location = 1) flat out vec4 o_EmissiveMetallness;
layout(location = 2) out vec3 o_Normal;
layout(location = 3) flat out int o_EntityID;
layout(location = 4) out vec2 o_TexCoords;
layout(location = 5) flat out uint o_AtlasIndex;
layout(location = 6) flat out float o_AO;
layout(location = 7) flat out float o_OpacityMask;
#ifdef EG_MOTION
layout(location = 8) out vec3 o_CurPos;
layout(location = 9) out vec3 o_PrevPos;
#endif

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 0.f, 1.0);

    o_AlbedoRoughness = a_AlbedoRoughness;
    o_EmissiveMetallness = a_EmissiveMetallness;

    const mat3 normalModel = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalModel * s_Normal);
    const bool bInvert = (gl_VertexIndex % 8u) < 4;
    if (bInvert)
        worldNormal = -worldNormal;
    o_Normal = worldNormal;

    o_TexCoords = a_TexCoords;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;
    o_AO = a_AO;
    o_OpacityMask = a_OpacityMask;

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;

    const mat4 prevModel = g_PrevTransforms[a_TransformIndex];
    const vec4 prevPos = g_PrevViewProjection * prevModel * vec4(a_Position, 0.f, 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
