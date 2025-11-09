#include "egpch.h"
#include "PhysicsShapes.h"
#include "PhysXInternal.h"
#include "PhysicsMaterial.h"
#include "PhysicsActor.h"
#include "PhysXCookingFactory.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	static physx::PxMaterial* GetMaterial_Internal(const Ref<AssetPhysicsMaterial>& materialAsset)
	{
		const Ref<PhysicsMaterial>& material = materialAsset ? materialAsset->GetMaterial() : PhysicsEngine::GetDefaultMaterial();
		return (physx::PxMaterial*)material->GetNativeHandle();
	}

	void ColliderShape::SetPhysicsMaterial(const Ref<AssetPhysicsMaterial>& materialAsset)
	{
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);
		m_Shape->setMaterials(&material, 1);
	}

	void ColliderShape::SetIsTrigger(bool bTrigger)
	{
		m_IsTrigger = bTrigger;
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

	void ColliderShape::SetCollisionEnabled(bool bEnabled)
	{
		using namespace physx;

		PxShapeFlag::Enum flag = m_IsTrigger ? PxShapeFlag::Enum::eTRIGGER_SHAPE : PxShapeFlag::Enum::eSIMULATION_SHAPE;
		m_Shape->setFlag(flag, bEnabled);
		m_Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bEnabled);
	}
	
	void ColliderShape::SetCollisionGroup(CollisionGroup group)
	{
		m_CollisionGroup = group;
		UpdateFilterData();
	}
	
	void ColliderShape::SetInteractingCollisionGroup(CollisionGroup group)
	{
		m_InteractingCollisionGroup = group;
		UpdateFilterData();
	}

	void ColliderShape::UpdateFilterData()
	{
		CollisionDetectionType collisionDetection = ((PhysicsActor*)m_Shape->getActor()->userData)->GetCollisionDetectionType();
		physx::PxFilterData filterData = PhysXUtils::GetPxFilterData(m_CollisionGroup, m_InteractingCollisionGroup, collisionDetection);
		m_Shape->setSimulationFilterData(filterData);
	}

	BoxColliderShape::BoxColliderShape(const BoxColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Box)
	{
		auto& physics = PhysXInternal::GetPhysics();
		const auto& materialAsset = component.GetPhysicsMaterialAsset();
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);

		m_ColliderScale = component.GetWorldTransform().Scale3D * component.GetSize();
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(m_ColliderScale.x * 0.5f, m_ColliderScale.y * 0.5f, m_ColliderScale.z * 0.5f);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *material);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(component.GetRelativeTransform()));
		m_Shape->userData = this;
		SetShowCollision(component.IsCollisionVisible());
		SetIsTrigger(component.IsTrigger());
		SetCollisionGroup(component.GetCollisionGroup());
		SetInteractingCollisionGroup(component.GetInteractingCollisionGroup());
		SetCollisionEnabled(component.IsCollisionEnabled());
	}

	void BoxColliderShape::SetSize(const glm::vec3& size)
	{
		const glm::vec3 absSize = glm::max(glm::vec3(0.000001f), glm::abs(size));
		if (m_ColliderScale == absSize)
			return;

		m_ColliderScale = absSize;
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(m_ColliderScale.x * 0.5f, m_ColliderScale.y * 0.5f, m_ColliderScale.z * 0.5f);
		m_Shape->setGeometry(geometry);
	}

	void BoxColliderShape::GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds) const
	{
		physx::PxBoxGeometry geometry{};
		if (m_Shape->getBoxGeometry(geometry) && geometry.isValid())
		{
			PhysXUtils::GetBoxGeometry(geometry, vertices, indices);
		}
	}

	SphereColliderShape::SphereColliderShape(const SphereColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Sphere)
	{
		auto& physics = PhysXInternal::GetPhysics();
		const auto& materialAsset = component.GetPhysicsMaterialAsset();
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);

		const auto& scale = component.GetWorldTransform().Scale3D;
		const float radius = scale.x * component.GetRadius();
		m_ColliderScale = glm::vec3(radius);

		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(radius);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *material);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(component.GetRelativeTransform()));
		m_Shape->userData = this;
		SetShowCollision(component.IsCollisionVisible());
		SetIsTrigger(component.IsTrigger());
		SetCollisionGroup(component.GetCollisionGroup());
		SetInteractingCollisionGroup(component.GetInteractingCollisionGroup());
		SetCollisionEnabled(component.IsCollisionEnabled());
	}

	void SphereColliderShape::SetRadius(float radius)
	{
		const float absRadius = glm::max(0.000001f, glm::abs(radius));
		if (m_ColliderScale.x == absRadius)
			return;

		m_ColliderScale = glm::vec3(absRadius);
		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(m_ColliderScale.x);
		m_Shape->setGeometry(geometry);
	}

	void SphereColliderShape::GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds) const
	{
		constexpr uint32_t numStacks = 48u;
		constexpr uint32_t numSlices = 48u;

		physx::PxSphereGeometry geometry{};
		if (m_Shape->getSphereGeometry(geometry) && geometry.isValid())
		{
			PhysXUtils::GetSphereGeometry(geometry, vertices, indices, numStacks, numSlices);
		}
	}
	
	CapsuleColliderShape::CapsuleColliderShape(const CapsuleColliderComponent& component, PhysicsActor& actor)
	: ColliderShape(ColliderType::Capsule)
	{
		auto& physics = PhysXInternal::GetPhysics();
		const auto& materialAsset = component.GetPhysicsMaterialAsset();
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);

		const auto& scale = component.GetWorldTransform().Scale3D;
		const float radius = scale.x * component.GetRadius();
		const float height = scale.y * component.GetHeight();
		m_ColliderScale = glm::vec3(radius, height, 1.f);

		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, height * 0.5f);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *material);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(component.GetRelativeTransform()));
		m_Shape->userData = this;

		SetShowCollision(component.IsCollisionVisible());
		SetIsTrigger(component.IsTrigger());
		SetCollisionGroup(component.GetCollisionGroup());
		SetInteractingCollisionGroup(component.GetInteractingCollisionGroup());
		SetCollisionEnabled(component.IsCollisionEnabled());
	}

	void CapsuleColliderShape::SetHeightAndRadius(float height, float radius)
	{
		const float absRadius = glm::max(0.000001f, glm::abs(radius));
		const float absHeight = glm::max(0.000001f, glm::abs(height));

		if (m_ColliderScale.x == absRadius && m_ColliderScale.y == absHeight)
			return;

		m_ColliderScale = glm::vec3(absRadius, absHeight, 1.f);
		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(m_ColliderScale.x, m_ColliderScale.y * 0.5f);
		m_Shape->setGeometry(geometry);
	}

	void CapsuleColliderShape::GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds) const
	{
		constexpr uint32_t numStacks = 48u;
		constexpr uint32_t numSlices = 48u;

		physx::PxCapsuleGeometry geometry{};
		if (m_Shape->getCapsuleGeometry(geometry) && geometry.isValid())
		{
			PhysXUtils::GetCapsuleGeometry(geometry, vertices, indices, numStacks, numSlices);
		}
	}
	
	ConvexMeshShape::ConvexMeshShape(const MeshColliderComponent& component, PhysicsActor& actor)
		: MeshShape(ColliderType::ConvexMesh)
	{
		bValid = component.IsConvex();
		EG_CORE_ASSERT(bValid, "Component is not Convex");

		if (!bValid)
			return;

		const auto& materialAsset = component.GetPhysicsMaterialAsset();
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);

		ScopedDataBuffer colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(component.GetCollisionMeshAsset(), component.IsConvex(), false, &colliderData);

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			bValid = false;
			return;
		}

		m_ColliderScale = component.GetWorldTransform().Scale3D;

		physx::PxDefaultMemoryInputData input((physx::PxU8*)colliderData.Data(), (physx::PxU32)colliderData.Size());
		m_ConvexMesh = PhysXInternal::GetPhysics().createConvexMesh(input);
		physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(m_ConvexMesh,
													 physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::Enum::eTIGHT_BOUNDS;

		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), convexGeometry, *material);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(component.GetRelativeTransform()));
		SetShowCollision(component.IsCollisionVisible());
		SetIsTrigger(component.IsTrigger());
		SetCollisionGroup(component.GetCollisionGroup());
		SetInteractingCollisionGroup(component.GetInteractingCollisionGroup());
		SetCollisionEnabled(component.IsCollisionEnabled());

		m_Shape->userData = this;
	}

	void ConvexMeshShape::SetScale(const glm::vec3& scale)
	{
		const glm::vec3 absScale = glm::max(glm::vec3(0.000001f), glm::abs(scale));
		if (m_ColliderScale == absScale)
			return;

		m_ColliderScale = absScale;
		physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(m_ConvexMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::Enum::eTIGHT_BOUNDS;

		m_Shape->setGeometry(convexGeometry);
	}

	void ConvexMeshShape::GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds) const
	{
		physx::PxConvexMeshGeometry geometry{};
		if (m_Shape->getConvexMeshGeometry(geometry) && geometry.convexMesh && geometry.isValid())
		{
			PhysXUtils::GetConvexMeshGeometry(geometry, vertices, indices);
		}
	}
	
	TriangleMeshShape::TriangleMeshShape(const MeshColliderComponent& component, bool bFlip, PhysicsActor& actor)
		: MeshShape(ColliderType::TriangleMesh)
	{
		bValid = !component.IsConvex();
		EG_CORE_ASSERT(bValid, "Component is Convex");
		
		if (!bValid)
			return;

		const auto& materialAsset = component.GetPhysicsMaterialAsset();
		physx::PxMaterial* material = GetMaterial_Internal(materialAsset);

		ScopedDataBuffer colliderData;
		CookingResult cookingResult = PhysXCookingFactory::CookMesh(component.GetCollisionMeshAsset(), component.IsConvex(), bFlip, &colliderData);

		if (cookingResult != CookingResult::Success)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking mesh failed");
			bValid = false;
			return;
		}

		m_ColliderScale = component.GetWorldTransform().Scale3D;

		physx::PxDefaultMemoryInputData input((physx::PxU8*)colliderData.Data(), (physx::PxU32)colliderData.Size());
		m_TriMesh = PhysXInternal::GetPhysics().createTriangleMesh(input);
		physx::PxTriangleMeshGeometry triGeometry = physx::PxTriangleMeshGeometry(m_TriMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));

		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), triGeometry, *material);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTranform(component.GetRelativeTransform()));
		SetShowCollision(component.IsCollisionVisible());
		SetIsTrigger(component.IsTrigger());
		SetCollisionGroup(component.GetCollisionGroup());
		SetInteractingCollisionGroup(component.GetInteractingCollisionGroup());
		SetCollisionEnabled(component.IsCollisionEnabled());

		m_Shape->userData = this;
	}

	void TriangleMeshShape::SetScale(const glm::vec3& scale)
	{
		const glm::vec3 absScale = glm::max(glm::vec3(0.000001f), glm::abs(scale));
		if (m_ColliderScale == absScale)
			return;

		m_ColliderScale = absScale;
		physx::PxTriangleMeshGeometry triGeometry = physx::PxTriangleMeshGeometry(m_TriMesh,
			physx::PxMeshScale(PhysXUtils::ToPhysXVector(m_ColliderScale)));
		m_Shape->setGeometry(triGeometry);
	}
	
	void TriangleMeshShape::GetGeometry(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds) const
	{
#if 0
		const auto& meshAsset = shape.GetCollisionMeshAsset();
		if (meshAsset)
		{
			const auto& mesh = meshAsset->GetMesh();
			const auto& meshVertices = mesh->GetVertices();
			vertices.reserve(meshVertices.size());
			indices.reserve(mesh->GetTotalIndicesCount());
			for (const auto& vertex : meshVertices)
				vertices.push_back(vertex.Position);
			for (uint32_t i = 0; i < mesh->GetMaterialSlotsCount(); ++i)
			{
				const auto& meshIndices = mesh->GetIndices(i);
				for (const auto& index : meshIndices)
					indices.push_back(index);
			}
		}
#else
		physx::PxTriangleMeshGeometry geometry{};
		if (m_Shape->getTriangleMeshGeometry(geometry) && geometry.triangleMesh && geometry.isValid())
		{
			PhysXUtils::GetTriangleMeshGeometry(geometry, vertices, indices);
		}
#endif
	}
}
