#pragma once

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

namespace Eagle
{
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
		: m_Material(nullptr), m_Type(type) {}

		virtual ~ColliderShape() = default;

		void Release()
		{
			m_Material->release();
		}

		void SetPhysicsMaterial(const Ref<PhysicsMaterial>& material);
		bool IsTrigger() const { return m_Shape->getFlags() & physx::PxShapeFlag::Enum::eTRIGGER_SHAPE; }
		void SetIsTrigger(bool bTrigger);
		virtual bool IsValid() const { return true; }
		void SetRelativeLocationAndRotation(const Transform& transform);
		void SetShowCollision(bool bShowCollision);

		void SetFilterData(const physx::PxFilterData& filterData) { m_Shape->setSimulationFilterData(filterData); };

		const physx::PxMaterial* GetMaterial() const { return m_Material; }
		const physx::PxShape* GetShape() const { return m_Shape; }
		physx::PxShape* GetShape() { return m_Shape; }

		//Returns scale3D used to create this shape
		const glm::vec3& GetColliderScale() const { return m_ColliderScale; }
	
	protected:
		void CreateMaterial(const Ref<PhysicsMaterial>& material);
	
	protected:
		glm::vec3 m_ColliderScale = glm::vec3{ 0.f };
		physx::PxShape* m_Shape = nullptr;
		physx::PxMaterial* m_Material;
		ColliderType m_Type;
	};

	class BoxColliderShape : public ColliderShape
	{
	public:
		BoxColliderShape(BoxColliderComponent& component, PhysicsActor& actor);
		~BoxColliderShape() = default;

		void SetSize(const glm::vec3& size);
	private:
		BoxColliderComponent& m_Component;
	};

	class SphereColliderShape : public ColliderShape
	{
	public:
		SphereColliderShape(SphereColliderComponent& component, PhysicsActor& actor);
		~SphereColliderShape() = default;

		void SetRadius(float radius);

	private:
		SphereColliderComponent& m_Component;
	};

	class CapsuleColliderShape : public ColliderShape
	{
	public:
		CapsuleColliderShape(CapsuleColliderComponent& component, PhysicsActor& actor);
		~CapsuleColliderShape() = default;

		void SetHeightAndRadius(float height, float radius);

	private:
		CapsuleColliderComponent& m_Component;
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
		ConvexMeshShape(MeshColliderComponent& component, PhysicsActor& actor);
		~ConvexMeshShape() { m_ConvexMesh->release(); };

		virtual bool IsValid() const override { return bValid; }
		virtual void SetScale(const glm::vec3& scale) override;

	private:
		MeshColliderComponent& m_Component;
		physx::PxConvexMesh* m_ConvexMesh = nullptr;
		bool bValid = true;
	};

	class TriangleMeshShape : public MeshShape
	{
	public:
		TriangleMeshShape(MeshColliderComponent& component, PhysicsActor& actor);
		~TriangleMeshShape() { m_TriMesh->release(); };

		virtual bool IsValid() const override { return bValid; }
		virtual void SetScale(const glm::vec3& scale) override;

	private:
		MeshColliderComponent& m_Component;
		physx::PxTriangleMesh* m_TriMesh = nullptr;
		bool bValid = true;
	};

}
