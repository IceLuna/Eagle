#pragma once

#include "Eagle/Math/AABB.h"
#include "Eagle/Math/Transform.h"
#include "Eagle/Core/GUID.h"
#include <glm/glm.hpp>

namespace Eagle
{
	class AssetTexture2D;
	class AssetStaticMesh;

	struct ParticleEmitter
	{
		enum class EmissionShapeType
		{
			Point, Sphere, SphereSurface, Box, Ring, Mesh
		};

		enum class CollisionModeType
		{
			None, DestroyOnHit, Bounce,
		};

		// ---------------- Particle properties ----------------
		Ref<AssetTexture2D> Texture;

		glm::vec4 ColorStart = glm::vec4(1.f);
		glm::vec4 ColorEnd = glm::vec4(1.f);
		
		glm::vec3 VelocityMin = glm::vec3(0, 1, 0);
		glm::vec3 VelocityMax = glm::vec3(0, 1, 0);
		
		glm::vec3 VelocityCoefStart = glm::vec3(1);
		glm::vec3 VelocityCoefEnd = glm::vec3(1);

		float RotationZStart = 0.f;
		float RotationZEnd = 0.f;

		glm::vec2 SizeStart = glm::vec2(1);
		glm::vec2 SizeEnd = glm::vec2(0);
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
		uint32_t NumParticles = 1;
		float NumParticlesRatio = 1.f; // Can be used to control `NumParticles`
		float FastForwardTo = 0.f; // Allows to fast-forward the simulation to make it look like it was running for `FastForwardTo` seconds
		float RadialAcceleration = 0.f; // If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center
		float TangentialAcceleration = 0.f; // If it's negative, particles will move towards the center of the emitter in a spiral way. If positive, they move away from the center.
		float NormalVelocityFactor = 0.f; // If not 0, particle's initial velocity will be affected by `EmissionShapeType` normal direction

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
		Ref<AssetStaticMesh> MeshAsset; // TODO: Add support for animated skeletal meshes

		CollisionModeType CollisionMode = CollisionModeType::None;

		// Animation
		glm::uvec2 AnimationImagesNum = glm::uvec2(1u); // Horizontal & Vertical images count
		float AnimationSpeed = 1.f;

		bool bEmit = true;
		bool bExplode = false; // If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime
		bool bApplyGravity = false;
		bool bAlphaBlending = true;
		bool bAdditive = false;
		bool bBlendAnimation = true;

		bool operator== (const ParticleEmitter& other) const
		{
			return ID == other.ID; // Note: we're not checking for other params. If we do, then the `unordered_map` lookup will fail since it check for the equality of the key as well
		}
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
