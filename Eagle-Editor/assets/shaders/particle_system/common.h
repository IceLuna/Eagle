#ifndef EG_PARTICLE_SYSTEM_COMMON
#define EG_PARTICLE_SYSTEM_COMMON

#ifndef __cplusplus

#include "defines.h"
#include "random.h"

#define EG_PS_THREAD_SIZE 64

struct ParticleSystemData
{
	uvec2 AliveCount; // Pre/Post simulation count
	uint EmitCount;
	uint SimulateCount;
	uint DeadCount;
};

const uint ParticleCollisionType_None = 0;
const uint ParticleCollisionType_DestroyOnHit = 1;
const uint ParticleCollisionType_Bounce = 2;

const uint EmissionShapeType_Point = 0;
const uint EmissionShapeType_Sphere = 1;
const uint EmissionShapeType_SphereSurface = 2;
const uint EmissionShapeType_Box = 3;
const uint EmissionShapeType_Ring = 4;

#endif

#ifdef __cplusplus

using uint = uint32_t;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using uvec2 = glm::uvec2;

#endif

const uint Emitter_OneShot_Mask       = 1 << 0;
const uint Emitter_Explode_Mask       = 1 << 1;
const uint Emitter_ApplyGravity_Mask  = 1 << 2;
const uint Emitter_AlphaBlending_Mask = 1 << 3;
const uint Emitter_Enabled_Mask       = 1 << 4;

bool HasFlag(uint flags, uint mask)
{
	return (flags & mask) == mask;
}

struct Emitter
{
	vec3 AABBMin;
	uint CollisionType;

	vec3 AABBMax;
	uint EmissionShape;

	vec4 ColorStart;
	vec4 ColorEnd;

	vec3 VelocityMin;
	float RotationZStart;

	vec3 VelocityMax;
	uint NumParticles;

	vec3 SizeStart;
	float RotationZEnd;

	vec3 SizeEnd;
	float RadialAcceleration;

	vec3 RingRadius;
	float BouncinessMin;

	vec3 RingThickness;
	float TangentialAcceleration;

	vec3 ColliderSizeRatio;
	float BouncinessMax;

	vec3 SphereRadius;
	uint TransformIndex;

	vec3 BoxMin;
	float LifetimeMin;

	vec3 BoxMax;
	float LifetimeMax;

	vec3 VelocityCoefStart;
	uint Flags;

	vec3 VelocityCoefEnd;
	float FastForwardTo; // TODO:

	uvec2 AnimationImagesNum;
	float AnimationSpeed;
	uint TextureIndex;

	// This is internal data. Keep it at the end because during update only the data before it is being updated
	vec3 WorldPos; // First
	float DeltaTime;

	uint IsVisible;
	uint SpawnedSoFar; // Used for `OneShot` emitters
	float Padding0;
	float Padding1;
};

struct Particle
{
	vec4 Color;

	vec3 Size;
	float CurrentLifetime;

	vec3 Position;
	float Lifetime;
	
	vec3 Velocity;
	float Bounciness;

	vec3 VelocityCoef;
	float RotationZ;

	vec2 AnimationUV0;
	uint EmitterIndex;
	uint TextureIndex; // This could be stored just in Emitter. But it's here to avoid an addition read from emitters buffer just to get this index

	vec2 AnimationUV1;
	uvec2 AnimationSpriteCoord;
};

#ifndef __cplusplus

struct DrawArgs
{
	uint VertexCount;
	uint InstanceCount;
	uint FirstVertex;
	uint FirstInstance;
};

vec3 Emitter_GetParticlePosition(Emitter emitter, inout Random random)
{
	switch (emitter.EmissionShape)
	{
		case EmissionShapeType_Point: return vec3(0);
		case EmissionShapeType_Sphere: return Random_PointInSphere(random, emitter.SphereRadius);
		case EmissionShapeType_SphereSurface: return Random_PointOnSphere(random, emitter.SphereRadius);
		case EmissionShapeType_Box: return mix(emitter.BoxMin, emitter.BoxMax, Random_NextFloat3(random));
		case EmissionShapeType_Ring: return Random_PointInRing(random, emitter.RingRadius, emitter.RingThickness);
	}
	return vec3(0);
}

void Particle_CalculateAnimationUV(uvec2 coord, uvec2 animationImagesNum, out vec2 uv0, out vec2 uv1)
{
	const vec2 spriteSize = 1.f / vec2(animationImagesNum);
	uv0 = coord * spriteSize;
	uv1 = uv0 + spriteSize;
}

void Particle_AdvanceAnimation(inout uvec2 coord, uvec2 animationImagesNum)
{
	coord.x += 1u;

	const bool exceededWidth = coord.x >= animationImagesNum.x;
	if (exceededWidth)
	{
		coord.x = 0u;
		coord.y += 1u;
		const bool exceededHeight = coord.y >= animationImagesNum.y;
		if (exceededHeight)
			coord.y = 0u;
	}
}

#endif

#endif // EG_PARTICLE_SYSTEM_COMMON
