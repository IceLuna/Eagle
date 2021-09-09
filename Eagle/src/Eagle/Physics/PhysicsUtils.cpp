#include "egpch.h"
#include "PhysicsUtils.h"
#include "Eagle/Math/Math.h"

namespace Eagle
{
	CookingResult PhysXUtils::FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult)
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

	CookingResult PhysXUtils::FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
		case physx::PxTriangleMeshCookingResult::eSUCCESS: return CookingResult::Success;
		case physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE: return CookingResult::LargeTriangle;
		case physx::PxTriangleMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	physx::PxTransform PhysXUtils::ToPhysXTranform(const glm::mat4& transform)
	{
		glm::vec3 location, rotation, scale;
		Math::DecomposeTransformMatrix(transform, location, rotation, scale);
		
		physx::PxVec3 p = ToPhysXVector(location);
		physx::PxQuat q = ToPhysXQuat(glm::quat(rotation));
		
		return physx::PxTransform(p, q);
	}

	physx::PxTransform PhysXUtils::ToPhysXTranform(const Transform& transform)
	{
		return ToPhysXTranform(transform.Location, transform.Rotation);
	}
	
	physx::PxTransform PhysXUtils::ToPhysXTranform(const glm::vec3& location, const glm::vec3& rotation)
	{
		return physx::PxTransform(ToPhysXVector(location), ToPhysXQuat(glm::quat(rotation)));
	}
	
	physx::PxBroadPhaseType::Enum PhysXUtils::ToPhysXBroadphaseType(BroadphaseType type)
	{
		switch (type)
		{
			case BroadphaseType::SweepAndPrune:		return physx::PxBroadPhaseType::Enum::eSAP;
			case BroadphaseType::MultiBoxPrune:		return physx::PxBroadPhaseType::Enum::eMBP;
			case BroadphaseType::AutomaticBoxPrune: return physx::PxBroadPhaseType::Enum::eABP;
			default: return physx::PxBroadPhaseType::Enum::eABP;
		}
	}
	
	physx::PxFrictionType::Enum PhysXUtils::ToPhysXFrictionType(FrictionType type)
	{
		switch (type)
		{
			case FrictionType::Patch:			return physx::PxFrictionType::Enum::ePATCH;
			case FrictionType::OneDirectional:	return physx::PxFrictionType::Enum::eONE_DIRECTIONAL;
			case FrictionType::TwoDirectional:	return physx::PxFrictionType::Enum::eTWO_DIRECTIONAL;
			default: return physx::PxFrictionType::Enum::eONE_DIRECTIONAL;
		}
	}
}
