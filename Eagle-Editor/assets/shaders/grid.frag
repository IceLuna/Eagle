layout(push_constant) uniform PushData
{
    layout(offset = 64) float g_GridSize;
    layout(offset = 68) float g_GridScale;
};

layout(location = 0) in vec2 i_UV;
layout(location = 0) out vec4 g_Output;

float grid(vec2 uv, float res)
{
	const vec2 grid = fract(uv);
	return step(res, grid.x) * step(res, grid.y);
}

void main()
{
    const float x = grid(i_UV * g_GridScale, g_GridSize);
    const vec4 color = vec4(vec3(0.2f), 0.5f) * (1.f - x);

    if (color.a == 0.f)
        discard;

    g_Output = color;
}
