#define VERTEX_COUNT 6

vec2 positions[VERTEX_COUNT] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0, -1.0)
);

vec2 UVs[VERTEX_COUNT] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 0.0)
);

layout(push_constant) uniform PushData
{
    mat4 g_MVP;
};

#ifdef EG_JITTER
layout(binding = 0) uniform Jitter
{
    vec2 g_Jitter;
};
#endif

layout(location = 0) out vec2 o_UV;

void main()
{
    gl_Position = g_MVP * vec4(positions[gl_VertexIndex], 0.0, 1.0);
#ifdef EG_JITTER
    gl_Position.xy += g_Jitter * gl_Position.w;
#endif

    o_UV = UVs[gl_VertexIndex];
}
