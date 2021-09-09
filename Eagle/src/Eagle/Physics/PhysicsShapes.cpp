#include "egpch.h"
#include "PhysicsShapes.h"
#include "PhysXInternal.h"
#include "PhysicsMaterial.h"
#include "PhysicsActor.h"
#include "PhysXCookingFactory.h"

namespace Eagle
{
	void ColliderShape::SetMaterial(const Ref<PhysicsMaterial>& material)
	{
		if (m_Material != nullptr)
			m_Material->release();
			
		m_Material = PhysXInternal::GetPhysics().createMaterial(material->StaticFriction, material->DynamicFriction, material->Bounciness);
	}
	
	BoxColliderShape::BoxColliderShape(BoxColliderComponent& component, PhysicsActor& actor, Entity entity)
	: ColliderShape(ColliderType::Box), m_Component(component)
	{
		SetMaterial(m_Component.Material);

		glm::vec3 colliderSize = m_Component.GetWorldTransform().Scale3D * m_Component.Size;
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(colliderSize.x / 2.f, colliderSize.y / 2.f, colliderSize.z / 2.f);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
	}
	
	void BoxColliderShape::SetIsTrigger(bool bTrigger)
	{
		m_Component.IsTrigger = bTrigger;
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
	}
	
	void BoxColliderShape::SetFilterData(const physx::PxFilterData& filterData)
	{
		m_Shape->setSimulationFilterData(filterData);
	}
	
	void BoxColliderShape::DetachFromActor(physx::PxRigidActor* actor)
	{
		actor->detachShape(*m_Shape);
	}
	
	SphereColliderShape::SphereColliderShape(SphereColliderComponent& component, PhysicsActor& actor, Entity entity)
	: ColliderShape(ColliderType::Sphere), m_Component(component)
	{
		SetMaterial(m_Component.Material);

		const glm::vec3& colliderSize = m_Component.GetWorldTransform().Scale3D;
		float largestAxis = glm::max(colliderSize.x, glm::max(colliderSize.y, colliderSize.z));

		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestAxis * m_Component.Radius);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
	}
	
	void SphereColliderShape::SetIsTrigger(bool bTrigger)
	{
		m_Component.IsTrigger = bTrigger;
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
	}
	
	void SphereColliderShape::SetFilterData(const physx::PxFilterData& filterData)
	{
		m_Shape->setSimulationFilterData(filterData);
	}
	
	void SphereColliderShape::DetachFromActor(physx::PxRigidActor* actor)
	{
		actor->detachShape(*m_Shape);
	}
	
	CapsuleColliderShape::CapsuleColliderShape(CapsuleColliderComponent& component, PhysicsActor& actor, Entity entity)
	: ColliderShape(ColliderType::Capsule), m_Component(component)
	{
		SetMaterial(m_Component.Material);

		const glm::vec3& colliderSize = m_Component.GetWorldTransform().Scale3D;
		float radiusScale = glm::max(colliderSize.x, colliderSize.z);

		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radiusScale * m_Component.Radius, (m_Component.Height / 2.f) * colliderSize.y);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
	}
	
	void CapsuleColliderShape::SetIsTrigger(bool bTrigger)
	{
		m_Component.IsTrigger = bTrigger;
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
	}
	
	void CapsuleColliderShape::SetFilterData(const physx::PxFilterData& filterData)
	{
		m_Shape->setSimulationFilterData(filterData);
	}
	
	void CapsuleColliderShape::DetachFromActor(physx::PxRigidActor* actor)
	{
		actor->detachShape(*m_Shape);
	}
	
	ConvexMeshShape::ConvexMeshShape(MeshColliderComponent& component, PhysicsActor& actor, Entity entity)
		: ColliderShape(ColliderType::ConvexMesh), m_Component(component)
	{
		EG_CORE_ASSERT(m_Component.IsConvex, "Component is not Convex");

		SetMaterial(m_Component.Material);

		MeshColliderData colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(m_Component, false, colliderData);
		EG_CORE_ASSERT(colliderData.Size > 0, "Cooking failed");

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			return;
		}

		const Transform& meshTransform = m_Component.GetWorldTransform();

		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
		physx::PxConvexMesh* convexMesh = PhysXInternal::GetPhysics().createConvexMesh(input);
		physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(convexMesh, 
													 physx::PxMeshScale(PhysXUtils::ToPhysXVector(meshTransform.Scale3D)));

		convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::Enum::eTIGHT_BOUNDS;

		physx::PxShape* shape = PhysXInternal::GetPhysics().createShape(convexGeometry, *m_Material, true);
		shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
		shape->setLocalPose(PhysXUtils::ToPhysXTranform(meshTransform.Location, meshTransform.Rotation));

		actor.GetPhysXActor()->attachShape(*shape);

		shape->release();
		convexMesh->release();
		delete[] colliderData.Data;
	}

	void ConvexMeshShape::SetIsTrigger(bool bTrigger)
	{
		m_Component.IsTrigger = bTrigger;
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
	}

	void ConvexMeshShape::SetFilterData(const physx::PxFilterData& filterData)
	{
		m_Shape->setSimulationFilterData(filterData);
	}

	void ConvexMeshShape::DetachFromActor(physx::PxRigidActor* actor)
	{
		actor->detachShape(*m_Shape);
	}
	
	TriangleMeshShape::TriangleMeshShape(MeshColliderComponent& component, PhysicsActor& actor, Entity entity)
		: ColliderShape(ColliderType::TriangleMesh), m_Component(component)
	{
		EG_CORE_ASSERT(!m_Component.IsConvex, "Component is Convex");

		SetMaterial(m_Component.Material);

		MeshColliderData colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(m_Component, false, colliderData);
		EG_CORE_ASSERT(colliderData.Size > 0, "Cooking failed");

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			return;
		}

		const Transform& meshTransform = m_Component.GetWorldTransform();

		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
		physx::PxTriangleMesh* triMesh = PhysXInternal::GetPhysics().createTriangleMesh(input);
		physx::PxTriangleMeshGeometry triGeometry = physx::PxTriangleMeshGeometry(triMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(meshTransform.Scale3D)));

		physx::PxShape* shape = PhysXInternal::GetPhysics().createShape(triGeometry, *m_Material, true);
		shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
		shape->setLocalPose(PhysXUtils::ToPhysXTranform(meshTransform.Location, meshTransform.Rotation));

		actor.GetPhysXActor()->attachShape(*shape);

		shape->release();
		triMesh->release();
		delete[] colliderData.Data;
	}

	void TriangleMeshShape::SetIsTrigger(bool bTrigger)
	{
		m_Component.IsTrigger = bTrigger;
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, m_Component.IsTrigger);
	}

	void TriangleMeshShape::SetFilterData(const physx::PxFilterData& filterData)
	{
		m_Shape->setSimulationFilterData(filterData);
	}

	void TriangleMeshShape::DetachFromActor(physx::PxRigidActor* actor)
	{
		actor->detachShape(*m_Shape);
	}

}
