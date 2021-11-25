#pragma once

#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>
#include "Eagle/Components/Components.h"

namespace Eagle
{
	class PhysicsMaterial;
	class PhysicsActor;

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

		void SetMaterial(const Ref<PhysicsMaterial>& material);

		virtual bool IsTrigger() const = 0;
		virtual void SetIsTrigger(bool bTrigger) = 0;
		virtual bool IsValid() const { return true; }

		void SetFilterData(const physx::PxFilterData& filterData) { m_Shape->setSimulationFilterData(filterData); };
		void DetachFromActor(physx::PxRigidActor* actor) { actor->detachShape(*m_Shape); }

		const physx::PxMaterial* GetMaterial() const { return m_Material; }

	protected:
		physx::PxShape* m_Shape = nullptr;
		physx::PxMaterial* m_Material;
		ColliderType m_Type;
	};

	class BoxColliderShape : public ColliderShape
	{
	public:
		BoxColliderShape(BoxColliderComponent& component, PhysicsActor& actor);
		~BoxColliderShape() = default;

		bool IsTrigger() const override { return m_Component.IsTrigger; }
		void SetIsTrigger(bool bTrigger) override;

	private:
		BoxColliderComponent& m_Component;
	};

	class SphereColliderShape : public ColliderShape
	{
	public:
		SphereColliderShape(SphereColliderComponent& component, PhysicsActor& actor);
		~SphereColliderShape() = default;

		bool IsTrigger() const override { return m_Component.IsTrigger; }
		void SetIsTrigger(bool bTrigger) override;

	private:
		SphereColliderComponent& m_Component;
	};

	class CapsuleColliderShape : public ColliderShape
	{
	public:
		CapsuleColliderShape(CapsuleColliderComponent& component, PhysicsActor& actor);
		~CapsuleColliderShape() = default;

		bool IsTrigger() const override { return m_Component.IsTrigger; }
		void SetIsTrigger(bool bTrigger) override;

	private:
		CapsuleColliderComponent& m_Component;
	};

	class ConvexMeshShape : public ColliderShape
	{
	public:
		ConvexMeshShape(MeshColliderComponent& component, PhysicsActor& actor);
		~ConvexMeshShape() = default;

		bool IsTrigger() const override { return m_Component.IsTrigger; }
		void SetIsTrigger(bool bTrigger) override;
		virtual bool IsValid() const override { return bValid; }

	private:
		MeshColliderComponent& m_Component;
		bool bValid = true;
	};

	class TriangleMeshShape : public ColliderShape
	{
	public:
		TriangleMeshShape(MeshColliderComponent& component, PhysicsActor& actor);
		~TriangleMeshShape() = default;

		bool IsTrigger() const override { return m_Component.IsTrigger; }
		void SetIsTrigger(bool bTrigger) override;
		virtual bool IsValid() const override { return bValid; }

	private:
		MeshColliderComponent& m_Component;
		bool bValid = true;
	};

}
