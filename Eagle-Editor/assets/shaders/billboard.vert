layout(location = 0) in vec3 a_ModelViewPos;
layout(location = 1) in vec3 a_PrevModelViewPos;
layout(location = 2) in uint a_TextureIndex;
layout(location = 3) in int  a_EntityID;

layout(push_constant) uniform PushConstants
{
    mat4 g_Proj;
    mat4 g_PrevProj;
};

#ifdef EG_JITTER
layout(set = 1, binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

layout(location = 0) out vec2 o_TexCoords;
layout(location = 1) flat out uint o_TextureIndex;
layout(location = 2) flat out int  o_EntityID;
#ifdef EG_MOTION
layout(location = 3) out vec3 o_CurPos;
layout(location = 4) out vec3 o_PrevPos;
#endif

const vec2 s_TexCoords[4] = {
	vec2(0.0f, 1.0f),
	vec2(1.f, 1.f),
	vec2(1.f, 0.f),
	vec2(0.f, 0.f)
};

void main()
{
    gl_Position = g_Proj * vec4(a_ModelViewPos, 1.0);

#ifdef EG_MOTION
    o_CurPos = gl_Position.xyw;
    const vec4 prevPos = g_PrevProj * vec4(a_PrevModelViewPos, 1.0);
    o_PrevPos = prevPos.xyw;
#endif

#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif

    const uint vertexID = gl_VertexIndex % 4u;
    o_TexCoords = s_TexCoords[vertexID];
    o_TextureIndex  = a_TextureIndex;
    o_EntityID = a_EntityID;
}
