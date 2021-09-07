#include "egpch.h"
#include "PhysicsUtils.h"
#include "Eagle/Math/Math.h"

namespace Eagle::PhysXUtils
{
	CookingResult FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
			case physx::PxConvexMeshCookingResult::eSUCCESS: return CookingResult::Success;
			case physx::PxConvexMeshCookingResult::eZERO_AREA_TEST_FAILED: return CookingResult::ZeroAreaTestFailed;
			case physx::PxConvexMeshCookingResult::ePOLYGONS_LIMIT_REACHED: return CookingResult::PolygonLimitReached;
			case physx::PxConvexMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	CookingResult FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
		case physx::PxTriangleMeshCookingResult::eSUCCESS: return CookingResult::Success;
		case physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE: return CookingResult::LargeTriangle;
		case physx::PxTriangleMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	physx::PxTransform ToPhysXTranform(const glm::mat4& transform)
	{
		glm::vec3 location, rotation, scale;
		Math::DecomposeTransformMatrix(transform, location, rotation, scale);
		
		physx::PxVec3 p = ToPhysXVector(location);
		physx::PxQuat q = ToPhysXQuat(glm::quat(rotation));
		
		return physx::PxTransform(p, q);
	}
	
	physx::PxTransform ToPhysXTranform(const glm::vec3& location, const glm::vec3& rotation)
	{
		return physx::PxTransform(ToPhysXVector(location), ToPhysXQuat(glm::quat(rotation)));
	}
}
