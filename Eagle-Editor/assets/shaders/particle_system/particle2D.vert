#include "particle_system/common.h"

vec2 s_Positions[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2(-0.5,  0.5),
    vec2( 0.5,  0.5),
    vec2( 0.5,  0.5),
    vec2( 0.5, -0.5),
    vec2(-0.5, -0.5)
);

const vec2 s_TexCoords[6] = {
	vec2(0.f, 1.f),
	vec2(0.f, 0.f),
	vec2(1.f, 0.f),
	vec2(1.f, 0.f),
	vec2(1.f, 1.f),
	vec2(0.f, 1.f)
};

layout(binding = 0) readonly buffer Particles
{
    Particle g_Particles[];
};

layout(binding = 1) readonly buffer IndicesToRender
{
    uint g_IndicesToRender[];
};

#ifdef EG_PARTICLE_BACK_TO_FRONT
layout(binding = 2) uniform DrawDataBuffer
{
    DrawArgs g_DrawArgs[2];
};
#endif

layout(push_constant) uniform PushConstants
{
    mat4 g_View;
    mat4 g_Proj;
};

layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec2 o_UV;
layout(location = 2) flat out uint o_TextureIndex;
#ifdef EG_BLEND
layout(location = 3) flat out uint o_Additive;
#endif

void main()
{
#ifdef EG_PARTICLE_BACK_TO_FRONT
    const uint particleIndex = g_IndicesToRender[g_DrawArgs[1].InstanceCount - gl_InstanceIndex - 1u];
#else
    const uint particleIndex = g_IndicesToRender[gl_InstanceIndex];
#endif
    const Particle particle = g_Particles[particleIndex];

    const vec2 uv = s_TexCoords[gl_VertexIndex] * (particle.AnimationUV1 - particle.AnimationUV0) + particle.AnimationUV0;

    o_Color = particle.Color;
    o_UV = uv;
    o_TextureIndex = particle.TextureIndex;
#ifdef EG_BLEND
    o_Additive = Particle_Unpack_IsAdditive(particle) ? 1u : 0u;
#endif

    const float rotationZ = particle.RotationZ;
    vec3 quadPos = vec3(s_Positions[gl_VertexIndex], 0.f);
    mat2 rotMat = mat2(cos(rotationZ), -sin(rotationZ), sin(rotationZ), cos(rotationZ));
    quadPos.xy = rotMat * quadPos.xy;
    quadPos.xy *= particle.Size.xy;

    vec4 position = g_View * vec4(particle.Position, 1.0);
    position.xyz += quadPos;

    gl_Position = g_Proj * position;
}
