layout(location = 0) in vec3 i_Pos;
layout(location = 1) flat in float i_Gamma;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_CubeMap;

void main()
{		
    vec3 color = texture(u_CubeMap, i_Pos).rgb;
    color = color / (color + vec3(1.f));
    color = pow(color, vec3(1.f / i_Gamma)); // Applying gamma correction because by default HDR-images are in linear space

    outColor = vec4(color, 1.0);
}
