#ifndef EG_PARTICLE_SYSTEM_COMMON
#define EG_PARTICLE_SYSTEM_COMMON

#ifndef __cplusplus

#include "defines.h"
#include "random.h"
#include "utils.h"
#include "noise.h"

#define EG_PS_THREAD_SIZE 64

struct ParticleSystemData
{
	uvec2 AliveCount; // Pre/Post simulation count
	uint EmitCount;
	uint SimulateCount;
	uint DeadCount;
	uint DroppedCount; // Particles that couldn't be spawned this frame because the pool was full. Read back by the CPU to grow the pool
	uint EventSpawnBudget; // Particles spawned by sub-emitter events this frame
	uint Padding0; // Keeps `CullingView` at offset 32 in C++ too

	// The culling camera of this frame, for per-particle culling (copied by `prepare_data` from its push constants)
	mat4 CullingView;
	vec4 CullingFrustum; // NearRight, NearTop, NearPlane, FarPlane (see `CullingFrustum`; the planes are negative view-space z)
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

const uint Emitter_MaxSubEmitters = 4; // Per emitter. Must match `ParticleEmitter::MaxSubEmitters`
const uint ParticleEvents_Capacity = 16384; // Max events per frame

const uint Emitter_Explode_Mask             = 1 << 0;
const uint Emitter_ApplyGravity_Mask        = 1 << 1;
const uint Emitter_AlphaBlending_Mask       = 1 << 2;
const uint Emitter_Enabled_Mask             = 1 << 3;
const uint Emitter_AdditiveBlending_Mask    = 1 << 4;
const uint Emitter_BlendAnimation_Mask      = 1 << 5;
const uint Emitter_DestroyImmediately_Mask  = 1 << 6;
const uint Emitter_FaceDirection_Mask       = 1 << 7;
const uint Emitter_SkeletalMesh_Mask        = 1 << 8;
const uint Emitter_WorldSpaceVelocity_Mask  = 1 << 9;
const uint Emitter_LocalSimulation_Mask     = 1 << 10;
const uint Emitter_HasSubEmitters_Mask      = 1 << 11; // Its particles can spawn particles of other emitters
const uint Emitter_SpawnOnlyFromEvents_Mask = 1 << 12; // Doesn't spawn on its own, only through sub-emitter events
const uint Emitter_PerParticleCulling_Mask  = 1 << 13; // Each particle is tested against the frustum, instead of the emitter's bounds
const uint Emitter_HasTurbulence_Mask       = 1 << 14;
const uint Emitter_ApplyDepthFade_Mask      = 1 << 15; // Translucent particles fade out near the geometry behind them
const uint Emitter_CameraFade_Mask          = 1 << 16; // Translucent particles fade out near the camera

const uint SubEmitterTrigger_None      = 0;
const uint SubEmitterTrigger_Death     = 1;
const uint SubEmitterTrigger_Collision = 2;

const uint SubEmitter_InheritColor_Mask = 1 << 0;

// A link from an emitter to one of its sub-emitters
struct SubEmitterLink
{
	uint TargetEmitterRef; // The sub-emitter index + generation
	uint Trigger;
	uint CountMin;
	uint CountMax;
	float Probability;
	float InheritVelocity;
	uint Flags;
	uint Padding0;
};

// An event that spawns particles of a sub-emitter
struct ParticleEvent
{
	vec3 Position;
	uint EmitterRef; // The sub-emitter
	vec3 Velocity; // The inherited part of the particle's velocity
	uint Count; // Particles to spawn
	vec4 Color; // The inherited color
};

const uint Emitter_Internal_IsVisible_Mask   = 1 << 0;
const uint Emitter_Internal_WasExplode_Mask  = 1 << 1; // Used to handle `bExplode` correctly
const uint Emitter_Internal_FastForward_Mask = 1 << 2; // Set for one frame. This frame's emitted particles are the fast-forwarded
const uint Emitter_Internal_HasPreviousFrame_Mask = 1 << 3; // `PreviousWorldPos` and `Velocity` are valid

// Every time an emitter slot is reused, its generation is bumped, so particles that still belong
// to the previous owner of the slot can detect it and die instead of using the new emitter's params.
const uint Emitter_IndexBits      = 24;
const uint Emitter_MaxIndex       = (1u << Emitter_IndexBits) - 1u;
const uint Emitter_GenerationMask = 0xFFu;

const uint Particle_Additive_Mask = 1 << 0;
const uint Particle_BlendAnimation_Mask = 1 << 1;
const uint Particle_FaceDirection_Mask  = 1 << 2;
const uint Particle_JustSpawned_Mask    = 1 << 3;

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

	vec3 VelocityMin;
	uint SpawnRate; // Particles per second

	vec3 VelocityMax;
	uint Flags;

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

	vec2 ColliderSizeRatio;
	uint LoopCount;
	float LoopDuration;

	uvec2 AnimationImagesNum;
	float AnimationSpeed;
	uint TextureIndex;

	float NormalVelocityFactor;
	// Used when EmissionShape is Mesh
	uint VertexOffset;
	uint IndexOffset;
	uint IndexCount;

	float BouncinessMax;
	uint AnimationOffset; // Used to retrieve animation data when skeletal mesh animation is used
	float RadialAcceleration;
	uint Generation; // Some bits are not used, see Emitter_GenerationMask

	vec4 RandomTintA; // The particle's color is multiplied by a random color between A and B
	vec4 RandomTintB;
	vec2 StartRotationRandomRange; // Radians (min, max)
	vec2 StartRotationSpeedRandomRange; // Radians per second (min, max)

	vec2 StartSizeMultiplierRandomRange; // (min, max). Each particle's size is multiplied by a random value in this range
	float InheritVelocity; // Fraction of the emitter's velocity that particles start with
	float SpawnPerMeter; // Extra particles per unit of distance the emitter moves (for continuous emitters)

	vec2 CameraFadeDistance; // World units from the camera. Fully faded at x, fully visible from y
	float NoiseFrequency; // 1 / swirl size at the start of the lifetime. Used to drift the pattern
	float NoiseScrollSpeed; // World units per second

	float DepthFadeDistance;
	uint Padding0;
	uint Padding1;
	uint Padding2;

	// This is internal data. Keep it at the end because during update only the data before it is being updated
	vec3 WorldPos; // First
	float DeltaTime;

	float SpawnIntervalTimer;
	uint LoopIteration; // Current loop iteration. When reaches LoopCount, it won't spawn any particles
	uint InternalFlags;
	float FastForwardTime; // Set when the emitter is added, reset by the first `prepare_data` pass

	vec3 PreviousWorldPos;
	float DistanceAccumulator; // Distance moved that hasn't spawned particles yet
	vec3 Velocity; // World units per second
	uint Padding3;

	// How the emitter's transform changed during the last frame (`current * inverse(previous)`).
	// `simulate` applies it to the emitter's particles so they move with it
	mat4 DeltaTransform;
	mat4 PreviousTransformInverse;

	vec3 NoiseOffset;
	uint Padding4;
};

// Values over a particle's lifetime, baked on the CPU from the emitter's curves
// Every emitter slot owns `EmitterCurve_Count` curves of `EmitterCurve_SamplesCount` samples (vec4) in the curves buffer
const uint EmitterCurve_SamplesCount = 64;
const uint EmitterCurve_Color = 0;        // rgb - color multiplied by the intensity, a - alpha
const uint EmitterCurve_SizeRotation = 1; // xy - size, z - rotation Z (radians), w - spin from `RotationSpeed` (radians per second of lifetime)
const uint EmitterCurve_VelocityCoef_Drag = 2; // xyz - velocity coef, w - drag (per second)
const uint EmitterCurve_Velocity = 3; // xyz - velocity over lifetime, w - gravity on (1) or off (0)
const uint EmitterCurve_Turbulence = 4; // x - turbulence strength, y - turbulence frequency (1 / swirl size), zw - unused
const uint EmitterCurve_Count = 5;

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

#ifdef __cplusplus
void Emitter_SetFastForwardEnabled(Emitter& emitter, bool bFastForwardBatch)
#else
void Emitter_SetFastForwardEnabled(inout Emitter emitter, bool bFastForwardBatch)
#endif
{
	emitter.InternalFlags = SetFlag(emitter.InternalFlags, Emitter_Internal_FastForward_Mask, bFastForwardBatch);
}

bool Emitter_IsFastForwardEnabled(Emitter emitter)
{
	return HasFlag(emitter.InternalFlags, Emitter_Internal_FastForward_Mask);
}

struct PackedParticle
{
	vec2 Size;
	float CurrentLifetime;
	float Lifetime;

	vec3 Position;
	uint Flags;

	vec3 Velocity;
	uint Color; // R11G11B10

	vec3 EffectiveVelocity; // World-space velocity after `VelocityCoef` was applied
	uint Bounciness_Opacity; // packHalf2x16

	uint RotationZ_AnimationLerp; // packHalf2x16
	uint TextureIndex; // Texture index is stored here to avoid an addition read from emitters buffer just to get this index
	uint EmitterRef; // Emitter index + generation, see `Emitter_IndexBits`
	uint AnimationImagesNum; // Used to calculate SpriteSize, which is used to calculate UV1 from UV0 (uv1 = uv0 + spriteSize)

	vec2 SizeScale;
	float RotationZOffset; // Includes the particle's random start rotation
	uint AnimationSpriteCoord; // High 16 bits - x, rest - y

	float TintFactor; // Blend factor between `RandomTintA` and `RandomTintB`
	float RotationSpeed; // Radians per second
	uint InheritedColorRG; // packHalf2x16. Color inherited from the particle that spawned this one through a sub-emitter event
	uint InheritedColorBA;
};

#ifndef __cplusplus

uint packUint16(uvec2 v)
{
	return (v.x & 0xFFFFu) | (v.y << 16);
}

uvec2 unpackUint16(uint p)
{
	return uvec2(p & 0xFFFFu, (p >> 16) & 0xFFFFu);
}

struct StaticMeshVertex
{
	vec3 Position;
	uint Normal;
};

struct SkeletalMeshVertex
{
	vec3 Position;
	uint Normal;
	uvec2 Weights;
	uvec2 BoneIDs;
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
	vec4 Color; // RGBA

	vec2 Size;
	float CurrentLifetime;
	float Lifetime;

	vec3 Position;
	uint Flags;

	vec3 Velocity;
	uint TextureIndex;

	vec3 EffectiveVelocity;
	uint EmitterRef; // Emitter index + generation, see `Emitter_IndexBits`

	vec2 AnimationUV0;
	vec2 AnimationUV1;
	vec2 NextAnimationUV0; // Used for lerping
	vec2 NextAnimationUV1; // Used for lerping

	uvec2 AnimationSpriteCoord;
	float AnimationLerp;
	float RotationZ;

	vec2 SizeScale;
	float RotationZOffset;
	float Bounciness;

	float TintFactor;
	float RotationSpeed;
	vec4 InheritedColor; // From a sub-emitter event
};

void Particle_CalculateAnimationUV(uvec2 coord, uvec2 animationImagesNum, out vec2 uv0, out vec2 uv1)
{
	const vec2 spriteSize = 1.f / vec2(animationImagesNum);
	uv0 = vec2(coord) * spriteSize;
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
			coord.y = animationImagesNum.y - 1u;
	}
}

// Same result as calling `Particle_AdvanceAnimation` `count` times
void Particle_AdvanceAnimationBy(inout uvec2 coord, uvec2 animationImagesNum, uint count)
{
	const uvec2 imagesNum = max(animationImagesNum, uvec2(1u));
	const uint frame = (coord.y * imagesNum.x + coord.x) + count;
	coord.x = frame % imagesNum.x;
	coord.y = min(frame / imagesNum.x, imagesNum.y - 1u);
}

PackedParticle Particle_Pack(Particle particle, uvec2 animationImagesNum)
{
	PackedParticle packed;

	packed.Size = particle.Size;
	packed.CurrentLifetime = particle.CurrentLifetime;
	packed.Lifetime = particle.Lifetime;

	packed.Position = particle.Position;
	packed.Flags = particle.Flags;

	packed.Velocity = particle.Velocity;
	packed.Color = PackR11G11B10(particle.Color.rgb);

	packed.EffectiveVelocity = particle.EffectiveVelocity;
	packed.Bounciness_Opacity = packHalf2x16(vec2(particle.Bounciness, particle.Color.a));

	packed.RotationZ_AnimationLerp = packHalf2x16(vec2(particle.RotationZ, particle.AnimationLerp));
	packed.EmitterRef = particle.EmitterRef;
	packed.TextureIndex = particle.TextureIndex;

	packed.AnimationImagesNum = packUint16(animationImagesNum);
	packed.AnimationSpriteCoord = packUint16(particle.AnimationSpriteCoord);

	packed.SizeScale = particle.SizeScale;
	packed.RotationZOffset = particle.RotationZOffset;

	packed.TintFactor = particle.TintFactor;
	packed.RotationSpeed = particle.RotationSpeed;
	packed.InheritedColorRG = packHalf2x16(particle.InheritedColor.rg);
	packed.InheritedColorBA = packHalf2x16(particle.InheritedColor.ba);

	return packed;
}

Particle Particle_Unpack(PackedParticle packed)
{
	Particle particle;

	particle.Size = packed.Size;
	particle.CurrentLifetime = packed.CurrentLifetime;
	particle.Lifetime = packed.Lifetime;

	particle.Position = packed.Position;
	particle.Flags = packed.Flags;

	particle.Velocity = packed.Velocity;
	particle.Color.rgb = UnpackR11G11B10(packed.Color);

	vec2 unpacked = unpackHalf2x16(packed.Bounciness_Opacity);
	particle.EffectiveVelocity = packed.EffectiveVelocity;
	particle.Bounciness = unpacked.x;
	particle.Color.a = unpacked.y;

	particle.SizeScale = packed.SizeScale;
	particle.RotationZOffset = packed.RotationZOffset;
	particle.TintFactor = packed.TintFactor;
	particle.RotationSpeed = packed.RotationSpeed;
	particle.InheritedColor = vec4(unpackHalf2x16(packed.InheritedColorRG), unpackHalf2x16(packed.InheritedColorBA));

	unpacked = unpackHalf2x16(packed.RotationZ_AnimationLerp);
	particle.RotationZ = unpacked.x;
	particle.AnimationLerp = unpacked.y;
	particle.TextureIndex = packed.TextureIndex;
	particle.EmitterRef = packed.EmitterRef;

	if (particle.TextureIndex != EG_INVALID_INDEX)
	{
		uvec2 animationImagesNum = unpackUint16(packed.AnimationImagesNum);
		uvec2 animationSpriteCoord = unpackUint16(packed.AnimationSpriteCoord);
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
		particle.AnimationSpriteCoord = uvec2(0);
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

uint Particle_MakeEmitterRef(uint emitterIndex, uint generation)
{
	return ((generation & Emitter_GenerationMask) << Emitter_IndexBits) | (emitterIndex & Emitter_MaxIndex);
}

uint Particle_GetEmitterIndex(uint emitterRef)
{
	return emitterRef & Emitter_MaxIndex;
}

uint Particle_GetEmitterGeneration(uint emitterRef)
{
	return emitterRef >> Emitter_IndexBits;
}

// Maps a float to a uint so that the uint order matches the float order (including negative values).
// Used to produce sort keys
uint FloatToSortableUint(float f)
{
	const uint bits = floatBitsToUint(f);
	return (bits & 0x80000000u) != 0u ? ~bits : (bits | 0x80000000u);
}

// Returns the rotation part of a TRS matrix (scale is removed)
mat3 ExtractRotation(mat4 m)
{
	const float sx = length(m[0].xyz);
	const float sy = length(m[1].xyz);
	const float sz = length(m[2].xyz);
	return mat3(
		sx > 1e-8f ? m[0].xyz / sx : vec3(1, 0, 0),
		sy > 1e-8f ? m[1].xyz / sy : vec3(0, 1, 0),
		sz > 1e-8f ? m[2].xyz / sz : vec3(0, 0, 1));
}

// `coef` is a per-axis multiplier defined in the emitter's local space, or in world space if `bWorldSpace` is set.
// `emitterRotation` must be orthonormal (see `ExtractRotation`)
vec3 ApplyVelocityCoef(vec3 worldVelocity, vec3 coef, mat3 emitterRotation, bool bWorldSpace)
{
	if (bWorldSpace || (coef.x == coef.y && coef.y == coef.z))
		return worldVelocity * coef; // World-space or uniform coef, no need to go to local space

	return emitterRotation * ((transpose(emitterRotation) * worldVelocity) * coef);
}

void Particle_ApplyDrag(inout vec3 velocity, float drag, float dt)
{
	if (drag > 0.f)
		velocity *= exp(-drag * dt);
}

// Must match `ParticleSystemTask::DecompositedTransform`
struct DecompositedTransform
{
	// World rotation without scale
	vec3 RotationColumn0;
	float RotationZ; // Radians
	vec3 RotationColumn1;
	float ScaleX;
	vec3 RotationColumn2;
	float ScaleY;
};

vec3 Particle_VelocityOverLifetimeToWorld(vec3 velocity, mat3 emitterRotation, bool bWorldSpaceVelocity)
{
	return bWorldSpaceVelocity ? velocity : emitterRotation * velocity;
}

mat3 DecompositedTransform_GetRotation(DecompositedTransform decomposited)
{
	return mat3(decomposited.RotationColumn0, decomposited.RotationColumn1, decomposited.RotationColumn2);
}

// Indices (in the curves buffer) of the two samples around `lifeAlpha`, and the blend factor between them
void EmitterCurve_GetSamples(uint emitterIndex, uint curve, float lifeAlpha, out uint index0, out uint index1, out float blend)
{
	const float x = clamp(lifeAlpha, 0.f, 1.f) * float(EmitterCurve_SamplesCount - 1u);
	const uint sample0 = min(uint(x), EmitterCurve_SamplesCount - 1u);
	const uint base = (emitterIndex * EmitterCurve_Count + curve) * EmitterCurve_SamplesCount;
	index0 = base + sample0;
	index1 = base + min(sample0 + 1u, EmitterCurve_SamplesCount - 1u);
	blend = x - float(sample0);
}

vec3 Emitter_GetNoiseDriftPerSecond(Emitter emitter)
{
	// Turbulence drift, in noise space per second. Along a fixed diagonal, so the swirls keep changing
	const vec3 noiseDriftDirection = vec3(0.31f, 1.f, 0.47f);
	return noiseDriftDirection * (emitter.NoiseScrollSpeed * emitter.NoiseFrequency);
}

// @turbulence. x - strength, y - frequency
vec3 Particle_ComputeForce(Emitter emitter, vec3 particlePosition, mat3 emitterRotation, vec3 gravity, vec3 noiseOffset, vec2 turbulence)
{
	vec3 force = vec3(0.f);
	if (HasFlag(emitter.Flags, Emitter_ApplyGravity_Mask))
		force += gravity;

	vec3 dir = particlePosition - emitter.WorldPos;
	if (dot(dir, dir) > 0.0001f) // if dir is not zero
	{
		dir = normalize(dir);
		force += dir * emitter.RadialAcceleration; // Radial
		force += cross(emitterRotation[2], dir) * emitter.TangentialAcceleration; // Tangential
	}

	// Turbulence
	const float turbulenceStrength = turbulence.x;
	const float noiseFrequency = turbulence.y;
	if (turbulenceStrength != 0.f)
		force += Noise_Curl(particlePosition * noiseFrequency + noiseOffset) * turbulenceStrength;
	return force;
}

// Index of the last spawn event at or before `FastForwardTime`
float FastForward_LastSpawnEventIndex(Emitter emitter, bool bExplode)
{
	const float time = emitter.FastForwardTime;
	if (bExplode)
	{
		float lastExplosion = floor(time / emitter.LoopDuration);
		if (emitter.LoopCount != 0)
			lastExplosion = min(lastExplosion, float(emitter.LoopCount - 1u));
		return lastExplosion;
	}

	// For limited loops, particle `k` spawns at `k / SpawnRate`, if that's before the end of the last loop (not at it)
	if (emitter.LoopCount != 0)
	{
		const float emissionEnd = float(emitter.LoopCount) * emitter.LoopDuration;
		if (time >= emissionEnd)
			return max(ceil(emissionEnd * float(emitter.SpawnRate) - 0.001f) - 1.f, 0.f);
	}
	return floor(time * float(emitter.SpawnRate));
}

float FastForward_SpawnEventTime(Emitter emitter, bool bExplode, float spawnEvent)
{
	return bExplode ? spawnEvent * emitter.LoopDuration : spawnEvent / float(emitter.SpawnRate);
}

// Age of the particle `particleIndex` within the fast-forwarded emit batch (0 = the youngest one)
float FastForward_GetParticleAge(Emitter emitter, uint particleIndex)
{
	const bool bExplode = HasFlag(emitter.Flags, Emitter_Explode_Mask);
	const float spawnEvent = FastForward_LastSpawnEventIndex(emitter, bExplode) - float(bExplode ? particleIndex / emitter.SpawnRate : particleIndex);
	return emitter.FastForwardTime - FastForward_SpawnEventTime(emitter, bExplode, spawnEvent);
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
