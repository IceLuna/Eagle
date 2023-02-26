#pragma once

#include <PhysX/PxPhysicsAPI.h>
#include "PhysicsSettings.h"
#include "Eagle/Core/Transform.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eagle
{
	enum class CookingResult
	{
		Success,
		ZeroAreaTestFailed,
		PolygonLimitReached,
		LargeTriangle,
		Failure
	};

	enum class ForceMode
	{
		Force,
		Impulse,
		VelocityChange,
		Acceleration
	};

	enum class ActorLockFlag
	{
		PositionX = BIT(0), PositionY = BIT(1), PositionZ = BIT(2), Position = PositionX | PositionY | PositionZ,
		RotationX = BIT(3), RotationY = BIT(4), RotationZ = BIT(5), Rotation = RotationX | RotationY | RotationZ
	};

	class PhysXUtils
	{
	public:
		static glm::vec2 FromPhysXVector(const physx::PxVec2& vector) { return *(glm::vec2*)(&vector); }
		static glm::vec3 FromPhysXVector(const physx::PxVec3& vector) { return *(glm::vec3*)(&vector); }
		static glm::vec4 FromPhysXVector(const physx::PxVec4& vector) { return *(glm::vec4*)(&vector); }
		static Rotator FromPhysXQuat(const physx::PxQuat& quat) { return Rotator(glm::quat(quat.w, quat.x, quat.y, quat.z)); }

		static CookingResult FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult);
		static CookingResult FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult);

		static physx::PxTransform ToPhysXTranform(const glm::mat4& transform);
		static physx::PxTransform ToPhysXTranform(const Transform& transform);
		static physx::PxTransform ToPhysXTranform(const glm::vec3& location, const Rotator& rotation);
		static physx::PxVec2 ToPhysXVector(const glm::vec2& vector) { return *(physx::PxVec2*)(&vector); }
		static physx::PxVec3 ToPhysXVector(const glm::vec3& vector) { return *(physx::PxVec3*)(&vector); }
		static physx::PxVec4 ToPhysXVector(const glm::vec4& vector) { return *(physx::PxVec4*)(&vector); }
		static physx::PxQuat ToPhysXQuat(const Rotator& quat) { return physx::PxQuat(quat.GetQuat().x, quat.GetQuat().y, quat.GetQuat().z, quat.GetQuat().w); }

		static physx::PxBroadPhaseType::Enum ToPhysXBroadphaseType(BroadphaseType type);
		static physx::PxFrictionType::Enum ToPhysXFrictionType(FrictionType type);

	};
}
