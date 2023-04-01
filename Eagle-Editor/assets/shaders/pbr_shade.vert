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

layout(location = 0) out vec2 o_UV;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    o_UV = UVs[gl_VertexIndex];
}
