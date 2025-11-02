#pragma once

#include "PhysicsEngine.h"
#include "Eagle/Core/Entity.h"
#include "Eagle/Math/Transform.h"

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Eagle
{
	class ColliderShape;

	enum class CookingResult
	{
		Success,
		ZeroAreaTestFailed,
		PolygonLimitReached,
		LargeTriangle,
		Failure
	};

	struct SceneQueryHit
	{
		// The Entity of the body that was hit.
		Entity HitEntity;

		// The shape on the body that was hit.
		ColliderShape* Shape = nullptr;
		
		const physx::PxRigidActor* Body = nullptr;

		bool IsValid() const { return HitEntity; }

		bool operator< (const SceneQueryHit& other) const
		{
			return HitEntity < other.HitEntity;
		}
	};
	using QueryHits = std::vector<SceneQueryHit>;
	using UniqueQueryHits = std::set<SceneQueryHit>;

	// Callback used to process unbounded overlap scene queries.
	struct UnboundedOverlap : public physx::PxHitCallback<physx::PxOverlapHit>
	{
		UnboundedOverlap(QueryHits& hits);
		
		// physx::PxHitCallback<physx::PxOverlapHit> ...
		physx::PxAgain processTouches(const physx::PxOverlapHit* buffer, physx::PxU32 numHits) override;

		QueryHits& m_Results;

	private:
		physx::PxOverlapHit m_Hit{};
	};

	// Callback used to process unbounded overlap scene queries.
	struct UniqueUnboundedOverlap : public physx::PxHitCallback<physx::PxOverlapHit>
	{
		UniqueUnboundedOverlap(UniqueQueryHits& hits);
		
		// physx::PxHitCallback<physx::PxOverlapHit> ...
		physx::PxAgain processTouches(const physx::PxOverlapHit* buffer, physx::PxU32 numHits) override;

		UniqueQueryHits& m_Results;

	private:
		physx::PxOverlapHit m_Hit{};
	};

	// Helper class, responsible for filtering invalid collision candidates prior to more expensive narrow phase checks
	class PhysXQueryFilterCallback : public physx::PxQueryFilterCallback
	{
	public:
		PhysXQueryFilterCallback() = default;
		PhysXQueryFilterCallback(physx::PxQueryHitType::Enum hitType, CollisionGroup group, const std::set<GUID>* entitiesToIgnore = nullptr)
			: m_HitType(hitType)
			, m_CollisionGroupMask(uint32_t(group))
			, m_IgnoredEntities((entitiesToIgnore && !entitiesToIgnore->empty()) ? entitiesToIgnore : nullptr)
		{}

		// Performs game specific entity filtering
		physx::PxQueryHitType::Enum preFilter(
			const physx::PxFilterData& queryFilterData, const physx::PxShape* pxShape,
			const physx::PxRigidActor* actor, physx::PxHitFlags& queryTypes) override;

		// Unused, we're only pre-filtering at this time
		physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit) override
		{
			return physx::PxQueryHitType::eNONE;
		}

	private:
		const uint32_t m_CollisionGroupMask = uint32_t(-1);
		physx::PxQueryHitType::Enum m_HitType = physx::PxQueryHitType::eBLOCK;
		const std::set<GUID>* m_IgnoredEntities;
	};

	class PhysXUtils
	{
	public:
		static glm::vec2 FromPhysXVector(const physx::PxVec2& vector) { return *(glm::vec2*)(&vector); }
		static glm::vec3 FromPhysXVector(const physx::PxVec3& vector) { return *(glm::vec3*)(&vector); }
		static glm::vec4 FromPhysXVector(const physx::PxVec4& vector) { return *(glm::vec4*)(&vector); }
		static Rotator FromPhysXQuat(const physx::PxQuat& quat) { return Rotator(glm::quat(quat.w, quat.x, quat.y, quat.z)); }
		static Transform FromPhysXTransform(const physx::PxTransform& transform) { return Transform{FromPhysXVector(transform.p), FromPhysXQuat(transform.q)}; }

		static CookingResult FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult);
		static CookingResult FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult);

		static physx::PxTransform ToPhysXTranform(const glm::mat4& transform);
		static physx::PxTransform ToPhysXTranform(const Transform& transform);
		static physx::PxTransform ToPhysXTranform(const glm::vec3& location, const Rotator& rotation);
		static physx::PxTransform ToPhysXTranform(const glm::vec3& location);
		static physx::PxVec2 ToPhysXVector(const glm::vec2& vector) { return *(physx::PxVec2*)(&vector); }
		static physx::PxVec3 ToPhysXVector(const glm::vec3& vector) { return *(physx::PxVec3*)(&vector); }
		static physx::PxVec4 ToPhysXVector(const glm::vec4& vector) { return *(physx::PxVec4*)(&vector); }
		static physx::PxQuat ToPhysXQuat(const Rotator& quat) { return physx::PxQuat(quat.GetQuat().x, quat.GetQuat().y, quat.GetQuat().z, quat.GetQuat().w); }

		static physx::PxBroadPhaseType::Enum ToPhysXBroadphaseType(BroadphaseType type);
		static physx::PxFrictionType::Enum ToPhysXFrictionType(FrictionType type);

		static physx::PxFilterData GetPxFilterData(CollisionGroup group, CollisionGroup interactingGroup, CollisionDetectionType collisionDetection);
		static physx::PxQueryFilterData GetPxQueryFilterData(PhysicsQueryType type);

		static void GetBoxGeometry(const physx::PxBoxGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
		static void GetCapsuleGeometry(const physx::PxCapsuleGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const uint32_t stacks, const uint32_t slices);
		static void GetConvexMeshGeometry(const physx::PxConvexMeshGeometry& geometry, std::vector<glm::vec3>& vertices, [[maybe_unused]] std::vector<uint32_t>& indices);
		static void GetHeightFieldGeometry(const physx::PxHeightFieldGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds);
		static void GetSphereGeometry(const physx::PxSphereGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const uint32_t stacks, const uint32_t slices);
		static void GetTriangleMeshGeometry(const physx::PxTriangleMeshGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
	};
}
