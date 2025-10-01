#pragma once

#include "Eagle/Core/Entity.h"
#include "Eagle/Math/AABB.h"
#include "PhysicsEngine.h"
#include "PhysicsUtils.h"

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

namespace Eagle
{
	class AssetPhysicsMaterial;
	class PhysicsMaterial;
	class PhysicsActor;
	class BoxColliderComponent;
	class SphereColliderComponent;
	class CapsuleColliderComponent;
	class MeshColliderComponent;
	struct Transform;

	//TODO: Heightfield, Plane
	enum class ColliderType
	{
		Box, Sphere, Capsule, ConvexMesh, TriangleMesh
	};

	class ColliderShape
	{
	public:
		ColliderShape(ColliderType type)
		: m_Type(type) {}

		virtual ~ColliderShape() = default;

		void SetPhysicsMaterial(const Ref<AssetPhysicsMaterial>& materialAsset);
		bool IsTrigger() const { return m_Shape->getFlags() & physx::PxShapeFlag::Enum::eTRIGGER_SHAPE; }
		void SetIsTrigger(bool bTrigger);
		virtual bool IsValid() const { return true; }
		void SetRelativeLocationAndRotation(const Transform& transform);
		void SetShowCollision(bool bShowCollision);

		// Collision groups it belongs to. It can belong to different groups (use XOR to combine groups)
		void SetCollisionGroup(CollisionGroup group);
		CollisionGroup GetCollisionGroup() const { return m_CollisionGroup; }

		// Collision groups it can interact with
		void SetInteractingCollisionGroup(CollisionGroup groups);
		CollisionGroup GetInteractingCollisionGroup() const { return m_InteractingCollisionGroup; }

		void UpdateFilterData();

		Transform GetGlobalTransform() const { return PhysXUtils::FromPhysXTransform(m_Shape->getActor()->getGlobalPose()); }
		Transform GetLocalTransform() const { return PhysXUtils::FromPhysXTransform(m_Shape->getLocalPose()); }

		const physx::PxShape* GetShape() const { return m_Shape; }
		physx::PxShape* GetShape() { return m_Shape; }

		ColliderType GetType() const { return m_Type; }

		const glm::vec3& GetColliderScale() const { return m_ColliderScale; }

		virtual void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const = 0;
	
	protected:
		physx::PxShape* m_Shape = nullptr; // Note: it's not released manually since it's created as an Exclusive Shape
		glm::vec3 m_ColliderScale = glm::vec3{ 1.f };
		ColliderType m_Type;
		CollisionGroup m_CollisionGroup = s_DefaultCollisionGroup;
		CollisionGroup m_InteractingCollisionGroup = s_DefaultInteractingCollisionGroup;
	};
	
	class BoxColliderShape : public ColliderShape
	{
	public:
		BoxColliderShape(const BoxColliderComponent& component, PhysicsActor& actor);
		~BoxColliderShape() = default;

		void SetSize(const glm::vec3& size);
		void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const override;
	};

	class SphereColliderShape : public ColliderShape
	{
	public:
		SphereColliderShape(const SphereColliderComponent& component, PhysicsActor& actor);
		~SphereColliderShape() = default;

		void SetRadius(float radius);
		void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const override;
	};

	class CapsuleColliderShape : public ColliderShape
	{
	public:
		CapsuleColliderShape(const CapsuleColliderComponent& component, PhysicsActor& actor);
		~CapsuleColliderShape() = default;

		void SetHeightAndRadius(float height, float radius);
		void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const override;
	};

	class MeshShape : public ColliderShape
	{
	public:
		MeshShape(ColliderType type) : ColliderShape(type) {}
		virtual void SetScale(const glm::vec3& scale) = 0;
	};

	class ConvexMeshShape : public MeshShape
	{
	public:
		ConvexMeshShape(const MeshColliderComponent& component, PhysicsActor& actor);
		~ConvexMeshShape()
		{
			if (m_ConvexMesh)
				m_ConvexMesh->release();
		};

		virtual bool IsValid() const override { return bValid; }
		virtual void SetScale(const glm::vec3& scale) override;
		void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const override;

	private:
		physx::PxConvexMesh* m_ConvexMesh = nullptr;
		bool bValid = true;
	};

	class TriangleMeshShape : public MeshShape
	{
	public:
		TriangleMeshShape(const MeshColliderComponent& component, bool bFlip, PhysicsActor& actor);
		~TriangleMeshShape()
		{
			if (m_TriMesh)
				m_TriMesh->release();
		};

		virtual bool IsValid() const override { return bValid; }
		virtual void SetScale(const glm::vec3& scale) override;

		void GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds = nullptr) const override;

	private:
		physx::PxTriangleMesh* m_TriMesh = nullptr;
		bool bValid = true;
	};
}
