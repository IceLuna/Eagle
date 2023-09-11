layout(location = 0) in vec3 a_ModelViewPos;
layout(location = 1) in vec3 a_PrevModelViewPos;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in uint a_TextureIndex;
layout(location = 4) in int  a_EntityID;

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

    o_TexCoords = a_TexCoords;
    o_TextureIndex  = a_TextureIndex;
    o_EntityID = a_EntityID;
}
