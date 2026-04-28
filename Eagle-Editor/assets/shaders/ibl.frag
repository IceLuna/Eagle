layout(location = 0) in vec3 i_Pos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D u_EquirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    float phi = 0.0;
    float theta = asin(clamp(-v.y, -1.0, 1.0));

    // Avoid atan2(0,0)
    if (abs(v.x) > 1e-6 || abs(v.z) > 1e-6)
        phi = atan(v.z, v.x);

    vec2 uv = vec2(phi, theta);
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    // Workaround for fireflies. Some IBLs might be super bright which causes fireflies
    const vec3 hdrLimit = vec3(50.f);

    const vec2 uv = SampleSphericalMap(normalize(i_Pos));
    vec3 color = texture(u_EquirectangularMap, uv).rgb;
    color = min(color, hdrLimit);

    outColor = vec4(color, 1.0);
}
