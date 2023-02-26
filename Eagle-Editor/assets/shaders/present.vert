#define VERTEX_COUNT 6

vec2 positions[VERTEX_COUNT] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0),
    vec2(-1.0, -1.0)
);

vec2 UVs[VERTEX_COUNT] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(0.0, 0.0)
);

layout(location = 0) out vec2 o_UV;

layout(push_constant) uniform PushData
{
    uint g_FlipX;
    uint g_FlipY;
};

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    o_UV = UVs[gl_VertexIndex];

    if (g_FlipX > 0)
        o_UV.x = 1.f - o_UV.x;
    if (g_FlipY > 0)
        o_UV.y = 1.f - o_UV.y;
}
