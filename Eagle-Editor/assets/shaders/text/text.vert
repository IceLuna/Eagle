#include "text/text_unlit_vertex_input_layout.h"

layout(push_constant) uniform PushConstants
{
    mat4 g_ViewProj;
};

layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec2 o_TexCoords;
layout(location = 2) flat out int o_EntityID;
layout(location = 3) flat out uint o_AtlasIndex;
#ifdef EG_MOTION
layout(location = 4) out vec3 o_CurPos;
layout(location = 5) out vec3 o_PrevPos;
#endif

layout(binding = 0)
readonly buffer TransformsBuffer
{
    mat4 g_Transforms[];
};

#ifdef EG_MOTION
layout(binding = 1) readonly buffer PrevTransformsBuffer
{
    mat4 g_PrevTransforms[];
};

layout(set = 0, binding = 2) uniform CameraData
{
    mat4 View;
    mat4 InvViewProj;
    mat4 ViewProj;
    mat4 PrevViewProj;
    mat4 Proj;
    mat4 InvProj;
    mat4 PrevProj;
    mat4 PrevView;
    mat4 ViewProjUnjittered;
    mat4 PrevViewProjUnjittered;
} g_Camera;
#endif

void main()
{
    const mat4 model = g_Transforms[a_TransformIndex];
    gl_Position = g_ViewProj * model * vec4(a_Position, 0.f, 1.0);

    o_TexCoords = a_TexCoords;
    o_Color = a_Color;
    o_EntityID = a_EntityID;
    o_AtlasIndex = a_AtlasIndex;

#ifdef EG_MOTION
    const vec4 curPos = g_Camera.ViewProjUnjittered * model * vec4(a_Position, 0.f, 1.f);
    o_CurPos = curPos.xyw;

    const mat4 prevModel = g_PrevTransforms[a_TransformIndex];
    const vec4 prevPos = g_Camera.PrevViewProjUnjittered * prevModel * vec4(a_Position, 0.f, 1.f);
    o_PrevPos = prevPos.xyw;
#endif
}
