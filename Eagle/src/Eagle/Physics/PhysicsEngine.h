#pragma once

#include "PhysicsSettings.h"
#include "Eagle/Core/EnumUtils.h"

namespace Eagle
{
	enum class ForceMode
	{
		Force,
		Impulse,
		VelocityChange,
		Acceleration
	};

	enum class PhysicsBodyType
	{
		Static,
		Dynamic
	};
	
	enum class CollisionDetectionType
	{
		Discrete,
		Continuous,
		ContinuousSpeculative
	};

	enum class ActorLockFlag
	{
		None = 0,
		PositionX = BIT(0), PositionY = BIT(1), PositionZ = BIT(2), Position = PositionX | PositionY | PositionZ,
		RotationX = BIT(3), RotationY = BIT(4), RotationZ = BIT(5), Rotation = RotationX | RotationY | RotationZ
	};
	DECLARE_FLAGS(ActorLockFlag);

	enum class CollisionGroup : uint32_t
	{
		Object = BIT(0),
		Projectile = BIT(1),
	};
	DECLARE_FLAGS(CollisionGroup);
	static constexpr CollisionGroup s_CollisionGroupNone = (CollisionGroup)0;
	static constexpr CollisionGroup s_DefaultCollisionGroup = CollisionGroup::Object;
	static constexpr CollisionGroup s_DefaultInteractingCollisionGroup = CollisionGroup::Object | CollisionGroup::Projectile;
	static constexpr CollisionGroup s_CollisionGroupAny = CollisionGroup(0xFFFFFFFF);

	enum class PhysicsQueryType
	{
		Static  = BIT(0),
		Dynamic = BIT(1),
		AnyHit  = BIT(2),
	};
	DECLARE_FLAGS(PhysicsQueryType);

	enum class CharacterControllerShape
	{
		Box,
		Capsule,
	};

	enum class CharacterControllerCollisionFlags
	{
		None  = 0,      // No collision detected
		Sides = BIT(0), // Character is colliding to the sides.
		Up    = BIT(1), // Character has collision above.
		Down  = BIT(2), // Character has collision below.
	};
	DECLARE_FLAGS(CharacterControllerCollisionFlags);

	enum class CapsuleClimbingMode
	{
		Easy,		 // Standard mode, let the capsule climb over surfaces according to impact normal
		Constrained, // Constrained mode, try to limit climbing according to the step offset
	};

	struct CollisionInfo
	{
		glm::vec3 Position = glm::vec3(0.f);
		glm::vec3 Normal = glm::vec3(0.f);
		glm::vec3 Impulse = glm::vec3(0.f);
		glm::vec3 Force = glm::vec3(0.f);
	};

	// A collection of triangle data within a volume defined by an axis aligned bounding box.
	struct OverlapGeometryData
	{
	public:
		AABB ScanBounds;

		std::vector<glm::vec3> Vertices;
		std::vector<uint32_t> Indices;

		bool IsEmpty() const
		{
			return Vertices.empty();
		}
	};

	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool IsVisualDebuggingSupported();

		static const Ref<class PhysicsMaterial>& GetDefaultMaterial();
	};
}
