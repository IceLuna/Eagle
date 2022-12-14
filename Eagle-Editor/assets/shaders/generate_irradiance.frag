#include "defines.h"

layout(location = 0) in vec3 i_Pos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube u_Cubemap;

void main()
{		
    const vec3 normal = normalize(i_Pos);

    vec3 up = vec3(0.f, 1.f, 0.f);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    const float sampleDelta = 0.025f;
    const float _2PI = 2 * PI;
    const float halfPI = 0.5f * PI;

    uint samplesCount = 0;
    vec3 irradiance = vec3(0.f);
    for (float phi = 0.f; phi < _2PI; phi += sampleDelta)
    {
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);

        for (float theta = 0.f; theta < halfPI; theta += sampleDelta)
        {
            const float cosTheta = cos(theta);
            const float sinTheta = sin(theta);

            // spherical to cartesian (in tangent space)
            const vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            // tangent space to world
            const vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(u_Cubemap, sampleVec).rgb * sinTheta * cosTheta;
            samplesCount++;
        }
    }
    irradiance = (PI * irradiance) / float(samplesCount);

    outColor = vec4(irradiance, 1.f);
}
