#include "particle_system/common.h"
#include "utils.h"

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

layout(binding = 0) readonly buffer PackedParticles
{
    PackedParticle g_Particles[];
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
layout(location = 1) out vec4 o_UVs;
layout(location = 2) flat out uint o_TextureIndex;
layout(location = 3) flat out uint o_Flags;
layout(location = 4) out float o_AnimationLerp;

void main()
{
#ifdef EG_PARTICLE_BACK_TO_FRONT
    const uint particleIndex = g_IndicesToRender[g_DrawArgs[1].InstanceCount - gl_InstanceIndex - 1u];
#else
    const uint particleIndex = g_IndicesToRender[gl_InstanceIndex];
#endif
    const Particle particle = Particle_Unpack(g_Particles[particleIndex]);

    const vec2 uv0 = s_TexCoords[gl_VertexIndex] * (particle.AnimationUV1 - particle.AnimationUV0) + particle.AnimationUV0;
    const vec2 uv1 = s_TexCoords[gl_VertexIndex] * (particle.NextAnimationUV1 - particle.NextAnimationUV0) + particle.NextAnimationUV0;

    o_Color = vec4(particle.Color);
    o_UVs = vec4(uv0, uv1);
    o_TextureIndex = particle.TextureIndex;
    o_Flags = particle.Flags;
    o_AnimationLerp = float(particle.AnimationLerp);

    const float rotationZ = particle.RotationZ;
    vec3 quadPos = vec3(s_Positions[gl_VertexIndex], 0.f);
    mat2 rotMat = mat2(cos(rotationZ), -sin(rotationZ), sin(rotationZ), cos(rotationZ));
    quadPos.xy = rotMat * quadPos.xy;
    quadPos.xy *= particle.Size.xy;

    vec4 position = g_View * vec4(particle.Position, 1.0);
    position.xyz += quadPos;

    gl_Position = g_Proj * position;
}
