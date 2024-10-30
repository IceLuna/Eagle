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

	struct CollisionInfo
	{
		glm::vec3 Position = glm::vec3(0.f);
		glm::vec3 Normal = glm::vec3(0.f);
		glm::vec3 Impulse = glm::vec3(0.f);
		glm::vec3 Force = glm::vec3(0.f);
	};

	struct PhysicsActorPayload
	{
		void* Ptr = nullptr; // Either `PhysicsActor*` or `PhysicsRagdollActor*`
		bool bRagdoll = false;
	};

	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static const Ref<class PhysicsMaterial>& GetDefaultMaterial();
	};
}
