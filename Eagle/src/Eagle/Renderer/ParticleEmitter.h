#pragma once

#include "Eagle/Math/AABB.h"
#include "Eagle/Math/Transform.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Animation/Animation.h"
#include "Eagle/Curves/CurveProperty.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class AssetTexture2D;
	class AssetBaseMesh;
	class AssetAnimation;

	struct ParticleEmitter
	{
		enum class EmissionShapeType
		{
			Point, Sphere, SphereSurface, Box, BoxSurface, Ring, Mesh
		};

		enum class CollisionModeType
		{
			None, DestroyOnHit, Bounce,
		};

		enum class SimulationSpaceType
		{
			World, // Particles stay where they were spawned
			Local  // Particles move, turn and scale with the emitter
		};

		enum class VelocitySpaceType
		{
			Local, // Relative to the emitter, rotates and scales with it
			World  // Independent of the emitter's rotation and scale
		};

		// ---------------- Particle properties ----------------
		Ref<AssetTexture2D> Texture;

		// Values over a particle's lifetime. Each one is either a constant or a curve over normalized lifetime (0 - spawn, 1 - death)
		CurveProperty<glm::vec4> Color = CurveProperty<glm::vec4>(glm::vec4(1.f));
		CurveProperty<float> ColorIntensity = CurveProperty<float>(1.f); // Multiplier for `Color.rgb`
		CurveProperty<glm::vec2> Size = CurveProperty<glm::vec2>::FromStartEnd(glm::vec2(1.f), glm::vec2(0.f));
		CurveProperty<float> RotationZ = CurveProperty<float>(0.f); // Degrees
		CurveProperty<float> RotationSpeed = CurveProperty<float>(0.f); // Degrees per second. Spin that changes over the lifetime. Adds to `StartRotationSpeedRandomRange`
		CurveProperty<glm::vec3> VelocityCoef = CurveProperty<glm::vec3>(glm::vec3(1.f)); // Multiplies the velocity (in `VelocitySpace`)
		// Added to the particle's movement over its lifetime
		// It isn't accumulated into the particle's velocity, meaning when the curve goes back to 0, so does its effect. Scaled by `VelocityCoef`
		CurveProperty<glm::vec3> VelocityOverLifetime = CurveProperty<glm::vec3>(glm::vec3(0.f));

		glm::vec4 RandomTintA = glm::vec4(1.f); // When `bRandomTint` is set, each particle's color is multiplied by a random color between A and B
		glm::vec4 RandomTintB = glm::vec4(1.f);
		bool bRandomTint = false;

		glm::vec2 StartRotationRandomRange = glm::vec2(0.f); // Degrees (min, max). A random offset in this range is added to `RotationZ`
		glm::vec2 StartRotationSpeedRandomRange = glm::vec2(0.f); // Degrees per second (min, max). Each particle spins at a random speed in this range
		glm::vec2 StartSizeMultiplierRandomRange = glm::vec2(1.f); // (min, max). Each particle's size is multiplied by a random value in this range

		float InheritVelocity = 0.f; // Fraction of the emitter's own velocity that particles start with

		glm::vec3 VelocityMin = glm::vec3(0, 1, 0);
		glm::vec3 VelocityMax = glm::vec3(0, 1, 0);
		VelocitySpaceType VelocitySpace = VelocitySpaceType::Local; // Space of `VelocityMin/Max` and `VelocityCoef`

		glm::vec2 ColliderSizeRatio = glm::vec2(1); // Can be used to increase the size of a collider to prevent small and fast-moving particles from clipping through

		// In seconds
		float LifetimeMin = 1.f;
		float LifetimeMax = 1.f;

		float BouncinessMin = 1.f;
		float BouncinessMax = 1.f;

		// ---------------- Emitter properties ----------------
		std::string Name = "Emitter";
		GUID ID{};
		Transform RelativeTransform; // Relative to the particle system
		AABB VisibilityAABB = AABB(glm::vec3(-1.f), glm::vec3(1.f)); // If not visible by the camera, it's not rendered to improve perf
		SimulationSpaceType SimulationSpace = SimulationSpaceType::World;

		enum class CullingType
		{
			EmitterBounds, // The emitter's particles are drawn while `VisibilityAABB` (at the emitter's transform) is visible
			PerParticle    // Each particle is tested against the camera frustum. Useful for particles that end up far from the emitter (trails, sub-emitters)
		};
		CullingType Culling = CullingType::EmitterBounds; // Event-only emitters always use `PerParticle`

		// ---------------- Sub-emitter ----------------
		enum class SubEmitterTrigger
		{
			Death,    // The particle's lifetime ended, or a collision destroyed it
			Collision // The particle hit something
		};

		struct SubEmitter
		{
			GUID EmitterID = GUID(0, 0); // Another emitter of the same particle system
			SubEmitterTrigger Trigger = SubEmitterTrigger::Death;
			bool bUseEmitterSpawnSettings = true; // When enabled, uses spawn rates of the sub-emitter. Otherwise, `CountRange` is used
			glm::uvec2 CountRange = glm::uvec2(1u); // (min, max) particles spawned per event
			float Probability = 1.f; // Chance that an event spawns anything [0; 1]
			float InheritVelocity = 0.f; // Fraction of the particle's velocity that spawned particles start with
			bool bInheritColor = false; // Spawned particles are tinted with the particle's color. For example, useful for liquid/paint splashes. Particles with random or varied colors hit a surface and spawn droplets of the same color.
		};

		static constexpr uint32_t MaxSubEmitters = 4; // Per emitter. Must match `Emitter_MaxSubEmitters` on the shader side
		std::vector<SubEmitter> SubEmitters; // Up to `MaxSubEmitters`
		bool bSpawnOnlyFromEvents = false; // Doesn't spawn particles on its own, only through other emitters sub-emitter events
		// ---------------------------------------------

		uint32_t LoopCount = 0u; // 0 - infinity
		float LoopDuration = 1.f;
		float StartDelay = 0.f; // Seconds before the emitter starts spawning, counted from when it's added (spawned or restarted)
		uint32_t SpawnRate = 1; // How many particles to spawn in a second. Clamped to `MaxSpawnRate`
		uint32_t SpawnPerMeter = 0; // Extra particles per unit of distance the emitter moves (for continuous emitters)

		// Sanity limit for `SpawnRate`. The number of particles that can be alive at once is limited separately, by `SceneRendererSettings::MaxParticlesBudget`.
		static constexpr uint32_t MaxSpawnRate = 16u * 1024u * 1024u;
		float FastForwardTo = 0.f; // When the emitter is added, it starts as if it had already been running for `FastForwardTo` seconds

		// Turbulence. A curl noise force field that makes particles swirl naturally
		CurveProperty<float> TurbulenceStrength = CurveProperty<float>(0.f); // Acceleration, over the lifetime. 0 - off
		CurveProperty<float> TurbulenceScale = CurveProperty<float>(1.f); // Size of the swirls in world units
		float TurbulenceSpeed = 0.5f; // How fast the swirl pattern drifts, in world units per second. Shouldn't be a curve so that the pattern is shared by all particles

		CurveProperty<float> Drag = CurveProperty<float>(0.f); // Per second. Slows particles down physically, their velocity loses this fraction per second
		float RadialAcceleration = 0.f; // If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center
		float TangentialAcceleration = 0.f; // Particles will move away from the center of the emitter in a spiral way.
		float NormalVelocityFactor = 0.f; // Adds the emission shape's normal direction (scaled by this value) to the initial velocity. Always follows the emitter, regardless of `VelocitySpace`

		EmissionShapeType EmissionShape = EmissionShapeType::Point;
		// Sphere emission shape
		glm::vec3 SphereRadius = glm::vec3(0.5f);
		// Box emission shape
		glm::vec3 BoxMin = glm::vec3(-0.5f);
		glm::vec3 BoxMax = glm::vec3(0.5f);
		// Ring emission shape
		glm::vec3 RingRadius = glm::vec3(0.5f);
		glm::vec3 RingThickness = glm::vec3(0.1f);

		// Mesh emission shape
		Ref<AssetBaseMesh> MeshAsset;
		Ref<AssetAnimation> MeshAnimationAsset;
		float ClipPlaybackSpeed = 1.f;
		bool bClipLooping = true;
		bool bTriggerAnimationEvents = false;

		CollisionModeType CollisionMode = CollisionModeType::None;

		// Animation
		glm::uvec2 AnimationImagesNum = glm::uvec2(1u); // Horizontal & Vertical images count
		float AnimationSpeed = 1.f;

		bool bDestroyImmediately = false; // If set to true, particles will be destroyed immediately when emitter is disabled/destroyed (instead of following their lifetime)
		bool bEmit = true;
		bool bExplode = false; // If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime
		CurveProperty<bool> ApplyGravity = CurveProperty<bool>(false); // Can be switched on and off over the lifetime

		// Translucent particles only (`bAlphaBlending`)
		bool bDepthFade = false; // Fade out where particles get close to the geometry behind them, instead of cutting through it
		float DepthFadeDistance = 0.1f; // World units. Particles are fully faded where they touch the geometry, fully visible this far in front of it
		bool bCameraFade = false; // Fade out particles close to the camera, instead of popping when they cross the near plane
		glm::vec2 CameraFadeDistance = glm::vec2(0.1f, 0.2f); // World units from the camera. Fully faded at (or closer than) x, fully visible from y

		bool bAlphaBlending = true;
		bool bAdditive = false;
		bool bBlendAnimation = true;
		bool bFaceDirection = false; // When set to true, particles will face the velocity direction

		bool operator== (const ParticleEmitter& other) const
		{
			return ID == other.ID; // Note: we're not checking for other params. If we do, then the `unordered_map` lookup will fail since it check for the equality of the key as well
		}

		bool IsSkeletalMeshUsed() const;
	};
}

namespace std
{
	template <>
	struct hash<Eagle::ParticleEmitter>
	{
		std::size_t operator()(const Eagle::ParticleEmitter& emitter) const
		{
			return emitter.ID.GetHash();
		}
	};
}
