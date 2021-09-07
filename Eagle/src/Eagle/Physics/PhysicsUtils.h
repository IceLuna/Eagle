#pragma once

#include <PhysX/PxPhysicsAPI.h>

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
		LocationX = BIT(0), LocationY = BIT(1), LocationZ = BIT(2), Location = LocationX | LocationY | LocationZ,
		RotationX = BIT(3), RotationY = BIT(4), RotationZ = BIT(5), Rotation = RotationX | RotationY | RotationZ
	};

	namespace PhysXUtils
	{
		glm::vec2 FromPhysXVector(const physx::PxVec2& vector) { return *(glm::vec2*)(&vector); }
		glm::vec3 FromPhysXVector(const physx::PxVec3& vector) { return *(glm::vec3*)(&vector); }
		glm::vec4 FromPhysXVector(const physx::PxVec4& vector) { return *(glm::vec4*)(&vector); }
		physx::PxVec2 ToPhysXVector(const glm::vec2& vector) { return *(physx::PxVec2*)(&vector); }
		physx::PxVec3 ToPhysXVector(const glm::vec3& vector) { return *(physx::PxVec3*)(&vector); }
		physx::PxVec4 ToPhysXVector(const glm::vec4& vector) { return *(physx::PxVec4*)(&vector); }

		glm::quat FromPhysXQuat(const physx::PxQuat& quat) { return *(glm::quat*)(&quat); }
		physx::PxQuat ToPhysXQuat(const glm::quat quat) { return physx::PxQuat(quat.x, quat.y, quat.z, quat.w); }

		CookingResult FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult);
		CookingResult FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult);

		physx::PxTransform ToPhysXTranform(const glm::mat4& transform);
		physx::PxTransform ToPhysXTranform(const glm::vec3& location, const glm::vec3& rotation);

	}
}
