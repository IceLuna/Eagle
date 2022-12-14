layout(location = 0) in vec3 i_Pos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D u_EquirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(-v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{		
    vec2 uv = SampleSphericalMap(normalize(i_Pos));
    vec3 color = texture(u_EquirectangularMap, uv).rgb;
    color = color / (color + vec3(1.f));

    outColor = vec4(color, 1.0);
}
