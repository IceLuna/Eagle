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
		CurveProperty<glm::vec3> VelocityCoef = CurveProperty<glm::vec3>(glm::vec3(1.f)); // Multiplies the velocity (in `VelocitySpace`)

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
		uint32_t LoopCount = 0u; // 0 - infinity
		float LoopDuration = 1.f;
		uint32_t SpawnRate = 1; // How many particles to spawn in a second. Clamped to `MaxSpawnRate`

		// Sanity limit for `SpawnRate`. The number of particles that can be alive at once is limited separately, by `SceneRendererSettings::MaxParticlesBudget`.
		static constexpr uint32_t MaxSpawnRate = 16u * 1024u * 1024u;
		float FastForwardTo = 0.f; // When the emitter is added, it starts as if it had already been running for `FastForwardTo` seconds
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
		bool bApplyGravity = false;
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
