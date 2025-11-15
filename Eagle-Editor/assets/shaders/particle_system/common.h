#ifndef EG_PARTICLE_SYSTEM_COMMON
#define EG_PARTICLE_SYSTEM_COMMON

#ifndef __cplusplus

#include "defines.h"
#include "random.h"
#include "utils.h"

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
const uint EmissionShapeType_BoxSurface = 4;
const uint EmissionShapeType_Ring = 5;
const uint EmissionShapeType_Mesh = 6;

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

const uint Emitter_Explode_Mask            = 1 << 0;
const uint Emitter_ApplyGravity_Mask       = 1 << 1;
const uint Emitter_AlphaBlending_Mask      = 1 << 2;
const uint Emitter_Enabled_Mask            = 1 << 3;
const uint Emitter_AdditiveBlending_Mask   = 1 << 4;
const uint Emitter_BlendAnimation_Mask     = 1 << 5;
const uint Emitter_DestroyImmediately_Mask = 1 << 6;
const uint Emitter_FaceDirection_Mask      = 1 << 7;

const uint Emitter_Internal_IsVisible_Mask  = 1 << 0;
const uint Emitter_Internal_WasExplode_Mask = 1 << 1; // Used to handle `bExplode` correctly

const uint Particle_Additive_Mask = 1 << 0;
const uint Particle_BlendAnimation_Mask = 1 << 1;
const uint Particle_FaceDirection_Mask  = 1 << 2;

uint SetFlag(uint flags, uint mask, bool bSet)
{
	if (bSet)
	{
		flags |= mask;
	}
	else
	{
		flags &= (~mask);
	}

	return flags;
}

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
	uint SpawnRate; // Particles per second

	vec2 SizeStart;
	vec2 SizeEnd;

	vec2 ColliderSizeRatio;
	uint LoopCount;
	float LoopDuration;

	vec3 RingRadius;
	float BouncinessMin;

	vec3 RingThickness;
	float TangentialAcceleration;

	vec3 SphereRadius;
	uint TransformIndex;

	vec3 BoxMin;
	float LifetimeMin;

	vec3 BoxMax;
	float LifetimeMax;

	vec3 VelocityCoefStart;
	uint Flags;

	vec3 VelocityCoefEnd;
	float RotationZEnd;

	uvec2 AnimationImagesNum;
	float AnimationSpeed;
	uint TextureIndex;

	float NormalVelocityFactor;
	// Used when EmissionShape is Mesh
	uint VertexOffset;
	uint IndexOffset;
	uint IndexCount;

	// TODO: Pack it somewhere
	float BouncinessMax;
	uint AnimationOffset; // Used to retrieve animation data when skeletal mesh animation is used
	float RadialAcceleration;
	uint Padding0;

	// This is internal data. Keep it at the end because during update only the data before it is being updated
	vec3 WorldPos; // First
	float DeltaTime;

	float SpawnIntervalTimer;
	uint LoopIteration; // Current loop iteration. When reaches LoopCount, it won't spawn any particles
	uint InternalFlags;
	uint Padding1;
};

#ifdef __cplusplus
void Emitter_SetIsVisible(Emitter& emitter, bool bVisible)
#else
void Emitter_SetIsVisible(inout Emitter emitter, bool bVisible)
#endif
{
	emitter.InternalFlags = SetFlag(emitter.InternalFlags, Emitter_Internal_IsVisible_Mask, bVisible);
}

bool Emitter_IsVisible(Emitter emitter)
{
	return HasFlag(emitter.InternalFlags, Emitter_Internal_IsVisible_Mask);
}

#ifdef __cplusplus
void Emitter_SetWasExplode(Emitter& emitter, bool bWasExplode)
#else
void Emitter_SetWasExplode(inout Emitter emitter, bool bWasExplode)
#endif
{
	emitter.InternalFlags = SetFlag(emitter.InternalFlags, Emitter_Internal_WasExplode_Mask, bWasExplode);
}

bool Emitter_WasExplode(Emitter emitter)
{
	return HasFlag(emitter.InternalFlags, Emitter_Internal_WasExplode_Mask);
}

// TODO: Is it even worth it? 92+4padding bytes (packed) vs 122 bytes (unpacked). Unpacked will probably require more, since then it'd need to be aligned correctly
struct PackedParticle
{
	vec2 Size;
	float CurrentLifetime;
	float Lifetime;

	vec3 Position;
	uint Flags;

	vec3 Velocity;
	uint Color; // R11G11B10

	vec3 VelocityCoef;
	uint Bounciness_Opacity; // packHalf2x16

	uint RotationZ_AnimationLerp; // packHalf2x16
	uint16_t TextureIndex; // Texture index is stored here to avoid an addition read from emitters buffer just to get this index
	uint16_t EmitterIndex;
	uint AnimationImagesNum; // Used to calculate SpriteSize, which is used to calculate UV1 from UV0 (uv1 = uv0 + spriteSize)
	uint AnimationSpriteCoord; // High 16 bits - x, rest - y

	vec2 SizeScale;
	float RotationZOffset;
	uint Padding0;
};

#ifndef __cplusplus

struct StaticMeshVertex
{
	vec3 Position;
	uint Normal;
};

struct SkeletalMeshVertex
{
	vec3 Position;
	uint Normal;
	f16vec4 Weights;
	u16vec4 BoneIDs;
};

struct DrawArgs
{
	uint VertexCount;
	uint InstanceCount;
	uint FirstVertex;
	uint FirstInstance;
};

struct Particle
{
	vec2 Size;
	float CurrentLifetime;
	float Lifetime;

	vec3 Position;
	uint Flags;

	vec3 Velocity;
	float16_t Bounciness;
	float16_t RotationZ;

	vec3 VelocityCoef;
	uint16_t EmitterIndex;
	uint16_t TextureIndex;

	f16vec4 Color; // RGBA

	vec2 AnimationUV0;
	vec2 AnimationUV1;
	vec2 NextAnimationUV0; // Used for lerping
	vec2 NextAnimationUV1; // Used for lerping

	u16vec2 AnimationSpriteCoord;
	float16_t AnimationLerp;

	vec2 SizeScale;
	float RotationZOffset;
};

void Particle_CalculateAnimationUV(u16vec2 coord, u16vec2 animationImagesNum, out vec2 uv0, out vec2 uv1)
{
	const vec2 spriteSize = 1.f / vec2(animationImagesNum);
	uv0 = vec2(coord) * spriteSize;
	uv1 = uv0 + spriteSize;
}

void Particle_AdvanceAnimation(inout u16vec2 coord, u16vec2 animationImagesNum)
{
	coord.x += uint16_t(1u);

	const bool exceededWidth = coord.x >= animationImagesNum.x;
	if (exceededWidth)
	{
		coord.x = uint16_t(0u);
		coord.y += uint16_t(1u);
		const bool exceededHeight = coord.y >= animationImagesNum.y;
		if (exceededHeight)
			coord.y = animationImagesNum.y - uint16_t(1u);
	}
}

PackedParticle Particle_Pack(Particle particle, u16vec2 animationImagesNum)
{
	PackedParticle packed;

	packed.Size = particle.Size;
	packed.CurrentLifetime = particle.CurrentLifetime;
	packed.Lifetime = particle.Lifetime;

	packed.Position = particle.Position;
	packed.Flags = particle.Flags;

	packed.Velocity = particle.Velocity;
	packed.Color = PackR11G11B10_F16(particle.Color.rgb);

	packed.VelocityCoef = particle.VelocityCoef;
	packed.Bounciness_Opacity = packFloat2x16(f16vec2(particle.Bounciness, particle.Color.a));

	packed.RotationZ_AnimationLerp = packFloat2x16(f16vec2(particle.RotationZ, particle.AnimationLerp));
	packed.EmitterIndex = particle.EmitterIndex;
	packed.TextureIndex = particle.TextureIndex;

	packed.AnimationImagesNum = packUint2x16(animationImagesNum);
	packed.AnimationSpriteCoord = packUint2x16(particle.AnimationSpriteCoord);

	packed.SizeScale = particle.SizeScale;
	packed.RotationZOffset = particle.RotationZOffset;

	return packed;
}

Particle Particle_Unpack(PackedParticle packed)
{
	f16vec2 unpackedf16;

	Particle particle;

	particle.Size = packed.Size;
	particle.CurrentLifetime = packed.CurrentLifetime;
	particle.Lifetime = packed.Lifetime;

	particle.Position = packed.Position;
	particle.Flags = packed.Flags;

	particle.Velocity = packed.Velocity;
	particle.Color.rgb = UnpackR11G11B10_F16(packed.Color);

	unpackedf16 = unpackFloat2x16(packed.Bounciness_Opacity);
	particle.VelocityCoef = packed.VelocityCoef;
	particle.Bounciness = unpackedf16.x;
	particle.Color.a = unpackedf16.y;

	particle.SizeScale = packed.SizeScale;
	particle.RotationZOffset = packed.RotationZOffset;

	unpackedf16 = unpackFloat2x16(packed.RotationZ_AnimationLerp);
	particle.RotationZ = unpackedf16.x;
	particle.AnimationLerp = unpackedf16.y;
	particle.TextureIndex = packed.TextureIndex;
	particle.EmitterIndex = packed.EmitterIndex;

	if (particle.TextureIndex != EG_INVALID_INDEX)
	{
		u16vec2 animationImagesNum = unpackUint2x16(packed.AnimationImagesNum);
		u16vec2 animationSpriteCoord = unpackUint2x16(packed.AnimationSpriteCoord);
		particle.AnimationSpriteCoord = animationSpriteCoord;

		Particle_CalculateAnimationUV(animationSpriteCoord, animationImagesNum, particle.AnimationUV0, particle.AnimationUV1);
		if (HasFlag(particle.Flags, Particle_BlendAnimation_Mask))
		{
			Particle_AdvanceAnimation(animationSpriteCoord, animationImagesNum);
			Particle_CalculateAnimationUV(animationSpriteCoord, animationImagesNum, particle.NextAnimationUV0, particle.NextAnimationUV1);
		}
		else
		{
			particle.NextAnimationUV0 = vec2(0);
			particle.NextAnimationUV1 = vec2(0);
		}
	}
	else
	{
		particle.AnimationSpriteCoord = u16vec2(0);
		particle.AnimationUV0 = vec2(0);
		particle.AnimationUV1 = vec2(0);
		particle.NextAnimationUV0 = vec2(0);
		particle.NextAnimationUV1 = vec2(0);
	}

	return particle;
}

vec3 Particle_UnpackNormal(uint packed)
{
	return DecodeNormal(unpackHalf2x16(packed));
}

uint EmitterFlagsToParticleFlags(uint flags)
{
	uint result = 0u;
	if (HasFlag(flags, Emitter_AdditiveBlending_Mask))
		result |= Particle_Additive_Mask;
	if (HasFlag(flags, Emitter_BlendAnimation_Mask))
		result |= Particle_BlendAnimation_Mask;
	if (HasFlag(flags, Emitter_FaceDirection_Mask))
		result |= Particle_FaceDirection_Mask;

	return result;
}

#endif

#endif // EG_PARTICLE_SYSTEM_COMMON
