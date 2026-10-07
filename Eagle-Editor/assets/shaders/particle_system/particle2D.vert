#include "particle_system/common.h"
#include "utils.h"

const vec2 s_Positions[6] = vec2[](
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

#ifdef EG_BLEND
layout(binding = 2) readonly buffer EmittersBuffer
{
    Emitter g_Emitters[];
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

#ifdef EG_BLEND
layout(location = 5) out float o_ViewDistance;
layout(location = 6) flat out vec3 o_FadeParams; // x - depth fade distance (0 - off), y/z - camera fade from/to distance (y >= z - off)
layout(location = 7) flat out vec2 o_DepthParams; // Projection terms that turn a depth buffer value into a view distance
#endif

vec3 RotateTowardsVelocity(Particle particle, vec3 quadPos)
{
    const vec3 velocity = particle.EffectiveVelocity;
    const float speed = length(velocity);
    if (speed < 0.001f)
    {
        return quadPos;
    }

    const vec3 forward = velocity / speed; // Normalize `particle.Velocity`

    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    if (abs(dot(forward, worldUp)) > 0.99)
    {
        worldUp = vec3(1.0, 0.0, 0.0); // Prevent gimbal lock when aligned
    }

    // Build an orthonormal basis from forward vector
    const vec3 right = normalize(cross(worldUp, forward));
    const vec3 up = cross(forward, right);

    const mat3 rotation = mat3(up, forward, right);
    return rotation * quadPos;
}

void main()
{
    const uint particleIndex = g_IndicesToRender[gl_InstanceIndex];
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

    vec4 position = vec4(particle.Position, 1.0);
    if (HasFlag(particle.Flags, Particle_FaceDirection_Mask))
    {
        position.xyz += RotateTowardsVelocity(particle, quadPos);
        position = g_View * position;
    }
    else
    {
        position = g_View * position;
        position.xyz += quadPos;
    }

    gl_Position = g_Proj * position;

#ifdef EG_BLEND
    o_ViewDistance = -position.z; // The camera looks down -z
    const Emitter emitter = g_Emitters[Particle_GetEmitterIndex(particle.EmitterRef)];
    o_FadeParams.x = HasFlag(emitter.Flags, Emitter_ApplyDepthFade_Mask) ? emitter.DepthFadeDistance : 0.f;
    o_FadeParams.yz = HasFlag(emitter.Flags, Emitter_CameraFade_Mask) ? emitter.CameraFadeDistance : vec2(0.f);
    o_DepthParams = vec2(g_Proj[2][2], g_Proj[3][2]);
#endif
}
