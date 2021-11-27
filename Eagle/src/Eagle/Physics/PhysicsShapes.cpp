#include "egpch.h"
#include "PhysicsShapes.h"
#include "PhysXInternal.h"
#include "PhysicsMaterial.h"
#include "PhysicsActor.h"
#include "PhysXCookingFactory.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	void ColliderShape::CreateMaterial(const Ref<PhysicsMaterial>& material)
	{
		if (m_Material != nullptr)
			m_Material->release();
			
		m_Material = PhysXInternal::GetPhysics().createMaterial(material->StaticFriction, material->DynamicFriction, material->Bounciness);
	}

	void ColliderShape::SetPhysicsMaterial(const Ref<PhysicsMaterial>& material)
	{
		CreateMaterial(material);
		m_Shape->setMaterials(&m_Material, 1);
	}

	void ColliderShape::SetIsTrigger(bool bTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
	}

	void ColliderShape::SetRelativeLocationAndRotation(const Transform& transform)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(transform));
	}

	void ColliderShape::SetShowCollision(bool bShowCollision)
	{
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eVISUALIZATION, bShowCollision);
	}
	
	BoxColliderShape::BoxColliderShape(BoxColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Box), m_Component(component)
	{
		CreateMaterial(m_Component.GetPhysicsMaterial());
		bool bTrigger = m_Component.IsTrigger();

		m_ColliderScale = m_Component.GetWorldTransform().Scale3D * m_Component.GetSize();
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(m_ColliderScale.x / 2.f, m_ColliderScale.y / 2.f, m_ColliderScale.z / 2.f);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
		SetShowCollision(m_Component.IsCollisionVisible());
	}

	void BoxColliderShape::SetSize(const glm::vec3& size)
	{
		m_ColliderScale = m_Component.GetWorldTransform().Scale3D * m_Component.GetSize();
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(m_ColliderScale.x / 2.f, m_ColliderScale.y / 2.f, m_ColliderScale.z / 2.f);
		m_Shape->setGeometry(geometry);
	}
	
	SphereColliderShape::SphereColliderShape(SphereColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Sphere), m_Component(component)
	{
		CreateMaterial(m_Component.GetPhysicsMaterial());

		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		float largestAxis = glm::max(m_ColliderScale.x, glm::max(m_ColliderScale.y, m_ColliderScale.z));
		bool bTrigger = m_Component.IsTrigger();

		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestAxis * m_Component.GetRadius());
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
		SetShowCollision(m_Component.IsCollisionVisible());
	}

	void SphereColliderShape::SetRadius(float radius)
	{
		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		float largestAxis = glm::max(m_ColliderScale.x, glm::max(m_ColliderScale.y, m_ColliderScale.z));
		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestAxis * radius);
		m_Shape->setGeometry(geometry);
	}
	
	CapsuleColliderShape::CapsuleColliderShape(CapsuleColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Capsule), m_Component(component)
	{
		CreateMaterial(m_Component.GetPhysicsMaterial());

		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		float radiusScale = glm::max(m_ColliderScale.x, m_ColliderScale.z);
		bool bTrigger = m_Component.IsTrigger();

		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radiusScale * m_Component.GetRadius(), (m_Component.GetHeight() / 2.f) * m_ColliderScale.y);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
		SetShowCollision(m_Component.IsCollisionVisible());
	}

	void CapsuleColliderShape::SetHeightAndRadius(float height, float radius)
	{
		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		float radiusScale = glm::max(m_ColliderScale.x, m_ColliderScale.z);
		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radiusScale * radius, (height / 2.f) * m_ColliderScale.y);
		m_Shape->setGeometry(geometry);
	}
	
	ConvexMeshShape::ConvexMeshShape(MeshColliderComponent& component, PhysicsActor& actor)
		: MeshShape(ColliderType::ConvexMesh), m_Component(component)
	{
		bValid = m_Component.IsConvex();
		EG_CORE_ASSERT(bValid, "Component is not Convex");

		CreateMaterial(m_Component.GetPhysicsMaterial());

		MeshColliderData colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(m_Component, false, colliderData);

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			bValid = false;
			return;
		}

		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		bool bTrigger = m_Component.IsTrigger();

		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
		m_ConvexMesh = PhysXInternal::GetPhysics().createConvexMesh(input);
		physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(m_ConvexMesh,
													 physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::Enum::eTIGHT_BOUNDS;

		m_Shape = PhysXInternal::GetPhysics().createShape(convexGeometry, *m_Material, true);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
		SetShowCollision(m_Component.IsCollisionVisible());

		actor.GetPhysXActor()->attachShape(*m_Shape);
		m_Shape->release();
		delete[] colliderData.Data;
	}

	void ConvexMeshShape::SetScale(const glm::vec3& scale)
	{
		m_ColliderScale = scale;
		physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(m_ConvexMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::Enum::eTIGHT_BOUNDS;

		m_Shape->setGeometry(convexGeometry);
	}
	
	TriangleMeshShape::TriangleMeshShape(MeshColliderComponent& component, PhysicsActor& actor)
		: MeshShape(ColliderType::TriangleMesh), m_Component(component)
	{
		bValid = !m_Component.IsConvex();
		EG_CORE_ASSERT(bValid, "Component is Convex");

		CreateMaterial(m_Component.GetPhysicsMaterial());

		MeshColliderData colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(m_Component, false, colliderData);

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			bValid = false;
			return;
		}

		m_ColliderScale = m_Component.GetWorldTransform().Scale3D;
		bool bTrigger = m_Component.IsTrigger();

		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
		m_TriMesh = PhysXInternal::GetPhysics().createTriangleMesh(input);
		physx::PxTriangleMeshGeometry triGeometry = physx::PxTriangleMeshGeometry(m_TriMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		m_Shape = PhysXInternal::GetPhysics().createShape(triGeometry, *m_Material, true);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eSIMULATION_SHAPE, !bTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::Enum::eTRIGGER_SHAPE, bTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(m_Component.GetRelativeTransform()));
		SetShowCollision(m_Component.IsCollisionVisible());

		actor.GetPhysXActor()->attachShape(*m_Shape);
		m_Shape->release();
		delete[] colliderData.Data;
	}

	void TriangleMeshShape::SetScale(const glm::vec3& scale)
	{
		m_ColliderScale = scale;
		physx::PxTriangleMeshGeometry triGeometry = physx::PxTriangleMeshGeometry(m_TriMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));
		m_Shape->setGeometry(triGeometry);
	}
}
