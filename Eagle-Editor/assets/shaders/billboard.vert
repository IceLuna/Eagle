#include "billboard_vertex_input_layout.h"

layout(set = 1, binding = 0) uniform CameraData
{
	mat4 g_View;
	mat4 g_InvViewProj;
	mat4 g_ViewProj;
	mat4 g_PrevViewProj;
	mat4 g_Proj;
	mat4 g_InvProj;
	mat4 g_PrevProj;
	mat4 g_PrevView;
	mat4 g_ViewProjUnjittered;
	mat4 g_PrevViewProjUnjittered;
	mat4 g_ProjUnjittered;
	mat4 g_PrevProjUnjittered;
};

layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_TextureIndex;
layout(location = 2) flat out int  o_EntityID;
#ifdef EG_MOTION
layout(location = 3) out vec3 o_CurPos;
layout(location = 4) out vec3 o_PrevPos;
#endif

void main()
{
    gl_Position = g_Proj * vec4(a_ModelViewPos, 1.0);

#ifdef EG_MOTION
    const vec4 curPos = g_ProjUnjittered * vec4(a_ModelViewPos, 1.0);
    o_CurPos = curPos.xyw;

    const vec4 prevPos = g_PrevProjUnjittered * vec4(a_PrevModelViewPos, 1.0);
    o_PrevPos = prevPos.xyw;
#endif

    const uint vertexID = gl_VertexIndex % 4u;
    o_TexCoords = s_TexCoords[vertexID];
    o_TextureIndex  = a_TextureIndex;
    o_EntityID = a_EntityID;
}
