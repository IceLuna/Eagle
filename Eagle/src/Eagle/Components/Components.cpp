#include "egpch.h"
#include "Component.h"

#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"
#include "Eagle/Physics/PhysicsShapes.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Components.h"

namespace Eagle
{
	namespace Utils
	{
		// True if found
		static bool GetBoneWorldTransform(const SkeletalPose& pose, const BoneNode& node, bool bRagdoll, const glm::mat4& parentTransform, const std::string_view targetBoneName, Transform* outTransform)
		{
			const std::string& nodeName = node.Name;
			glm::mat4 globalTransformation;
			if (auto it = pose.Bones.find(nodeName); it != pose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				// If a ragdoll, then it's already a global transform
				globalTransformation = bRagdoll ? boneTransform : parentTransform * boneTransform;
			}
			else
				globalTransformation = parentTransform * node.Transformation;

			if (nodeName == targetBoneName)
			{
				*outTransform = Math::DecomposeTransformMatrix(globalTransformation);
				return true;
			}

			for (auto& child : node.Children)
				if (GetBoneWorldTransform(pose, child, bRagdoll, globalTransformation, targetBoneName, outTransform))
					return true;

			return false;
		}
	}

	void RigidBodyComponent::SetMass(float mass)
	{
		Mass = std::max(0.f, mass);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetMass(Mass);
	}

	void RigidBodyComponent::SetLinearDamping(float linearDamping)
	{
		LinearDamping = std::max(0.f, linearDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLinearDamping(LinearDamping);
	}

	void RigidBodyComponent::SetAngularDamping(float angularDamping)
	{
		AngularDamping = std::max(0.f, angularDamping);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetAngularDamping(AngularDamping);
	}

	void RigidBodyComponent::SetEnableGravity(bool bEnable)
	{
		bEnableGravity = bEnable;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetGravityEnabled(bEnable);
	}

	void RigidBodyComponent::SetIsKinematic(bool bKinematic)
	{
		this->bKinematic = bKinematic;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetKinematic(bKinematic);
	}

	void RigidBodyComponent::SetMaxLinearVelocity(float velocity)
	{
		MaxLinearVelocity = glm::max(0.f, velocity);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetMaxLinearVelocity(MaxLinearVelocity);
	}

	void RigidBodyComponent::SetMaxAngularVelocity(float velocity)
	{
		MaxAngularVelocity = glm::max(0.f, velocity);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetMaxAngularVelocity(MaxAngularVelocity);
	}

	void RigidBodyComponent::SetLockFlag(ActorLockFlag flag, bool value)
	{
		if (value)
			m_LockFlags |= flag;
		else
			m_LockFlags &= ~flag;

		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
	}

	void RigidBodyComponent::SetLockFlag(ActorLockFlag flag)
	{
		m_LockFlags = flag;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
	}

	void BaseColliderComponent::SetPhysicsMaterialAsset(const Ref<AssetPhysicsMaterial>& material)
	{
		if (material == m_MaterialAsset)
			return;

		m_MaterialAsset = material;
		UpdatePhysicsMaterials();
	}

	void BaseColliderComponent::SetWorldTransform(const Transform& worldTransform)
	{
		SceneComponent::SetWorldTransform(worldTransform);
		UpdatePhysicsTransform();
	}

	void BaseColliderComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		SceneComponent::SetRelativeTransform(relativeTransform);
		UpdatePhysicsTransform();
	}

	void BaseColliderComponent::SetIsObstacle(bool bValue)
	{
		if (bObstacle == bValue)
			return;

		bObstacle = bValue;
		if (bObstacle)
			CreateObstacle();
		else
			RemoveObstacle();
	}

	BaseColliderComponent& BaseColliderComponent::operator=(const BaseColliderComponent& other)
	{
		SceneComponent::operator=(other);
		SetPhysicsMaterialAsset(other.m_MaterialAsset);
		SetIsTrigger(other.bTrigger);
		SetShowCollision(other.bShowCollision);
		SetAffectsNavMeshBuild(other.bAffectsNavMeshBuild);
		SetIsObstacle(other.IsObstacle());

		return *this;
	}

	bool BaseColliderComponent::RemoveObstacle()
	{
		if (m_ObstacleID == 0u)
			return false;

		const Ref<AINavigation::Mesh>& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return false;

		if (navMesh->RemoveObstacle(m_ObstacleID))
		{
			m_ObstacleID = 0u;
			return true;
		}
		return false;
	}
	
	BoxColliderComponent& BoxColliderComponent::operator=(const BoxColliderComponent& other)
	{
		BaseColliderComponent::operator=(other);
		SetSize(other.m_Size);
		UpdatePhysicsTransform();

		return *this;
	}

	void BoxColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void BoxColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset);
	}

	void BoxColliderComponent::CreateObstacle()
	{
		if (m_ObstacleID != 0u)
		{
			if (RemoveObstacle() == false)
				return; // Remove failed, so we shouldn't create a new one.
		}

		const Ref<AINavigation::Mesh>& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		const glm::vec3 halfExtents = m_Shape->GetColliderScale() * 0.5f;
		AABB aabb(WorldTransform.Location - halfExtents, WorldTransform.Location + halfExtents);
		if (!AABB::Overlap(aabb, navMesh->GetAABB()))
			return; // Don't create if it doesn't overlap a nav mesh

		const float yRotation = WorldTransform.Rotation.EulerAngles().y;
		m_ObstacleID = navMesh->AddBoxObstacle(WorldTransform.Location, halfExtents, yRotation);
	}

	void BoxColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void BoxColliderComponent::OnInit(Entity entity)
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void BoxColliderComponent::OnRemoved(Entity entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}
	
	void BoxColliderComponent::SetSize(const glm::vec3& size)
	{
		m_Size = glm::max(size, glm::vec3(0.f));
		m_Shape->SetSize(WorldTransform.Scale3D * m_Size);
		if (IsObstacle())
			CreateObstacle();
	}

	void BoxColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3 size = WorldTransform.Scale3D * m_Size;
			m_Shape->SetSize(size);

			if (IsObstacle())
				CreateObstacle(); // Recreate obstacle
		}
	}

	SphereColliderComponent& SphereColliderComponent::operator=(const SphereColliderComponent& other)
	{
		BaseColliderComponent::operator=(other);
		SetRadius(other.m_Radius);
		UpdatePhysicsTransform();

		return *this;
	}

	void SphereColliderComponent::SetRadius(float radius)
	{
		m_Radius = glm::max(radius, 0.f);

		const auto& scale = WorldTransform.Scale3D;
		const float largestAxis = glm::max(scale.x, glm::max(scale.y, scale.z));
		m_Shape->SetRadius(largestAxis * m_Radius);
		if (IsObstacle())
			CreateObstacle();
	}
	
	void SphereColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void SphereColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset);
	}

	void SphereColliderComponent::CreateObstacle()
	{
		if (m_ObstacleID != 0u)
		{
			if (RemoveObstacle() == false)
				return; // Remove failed, so we shouldn't create a new one.
		}

		const Ref<AINavigation::Mesh>& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		const float radius = m_Shape->GetColliderScale().x;

		AABB aabb(WorldTransform.Location - radius, WorldTransform.Location + radius);
		if (!AABB::Overlap(aabb, navMesh->GetAABB()))
			return; // Don't create if it doesn't overlap a nav mesh

		const float height = radius * 2.f;
		m_ObstacleID = navMesh->AddCylinderObstacle(WorldTransform.Location - glm::vec3(0.f, radius, 0.f), radius, height);
	}

	void SphereColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void SphereColliderComponent::OnInit(Entity entity)
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void SphereColliderComponent::OnRemoved(Entity entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}

	void SphereColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const auto& scale = WorldTransform.Scale3D;
			const float largestAxis = glm::max(scale.x, glm::max(scale.y, scale.z));
			const float radius = largestAxis * m_Radius;
			m_Shape->SetRadius(radius);

			if (IsObstacle())
				CreateObstacle(); // Recreate obstacle
		}
	}

	CapsuleColliderComponent& CapsuleColliderComponent::operator=(const CapsuleColliderComponent& other)
	{
		BaseColliderComponent::operator=(other);
		SetHeightAndRadius(other.m_Height, other.m_Radius);
		UpdatePhysicsTransform();

		return *this;
	}

	void CapsuleColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void CapsuleColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset);
	}

	void CapsuleColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void CapsuleColliderComponent::SetHeightAndRadius(float height, float radius)
	{
		m_Height = glm::max(height, 0.f);
		m_Radius = glm::max(radius, 0.f);

		m_Shape->SetHeightAndRadius(height, radius);
		if (IsObstacle())
			CreateObstacle();
	}
	
	void CapsuleColliderComponent::OnInit(Entity entity)
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
		m_Shape->SetFilterData(actor->GetFilterData());
	}

	void CapsuleColliderComponent::CreateObstacle()
	{
		if (m_ObstacleID != 0u)
		{
			if (RemoveObstacle() == false)
				return; // Remove failed, so we shouldn't create a new one.
		}

		const Ref<AINavigation::Mesh>& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		const glm::vec3& scale = m_Shape->GetColliderScale();
		const float& radius = scale.x;
		const float& height = scale.y;

		const glm::vec3 halfExtent1 = glm::vec3(radius, 0.f, radius);
		const glm::vec3 halfExtent2 = glm::vec3(radius, height, radius);
		AABB aabb(WorldTransform.Location - halfExtent1, WorldTransform.Location + halfExtent2);
		if (!AABB::Overlap(aabb, navMesh->GetAABB()))
			return; // Don't create if it doesn't overlap a nav mesh

		m_ObstacleID = navMesh->AddCylinderObstacle(WorldTransform.Location, radius, height);
	}

	void CapsuleColliderComponent::OnRemoved(Entity entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}

	void CapsuleColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const auto& scale = WorldTransform.Scale3D;
			const float radius = glm::max(scale.x, scale.z) * m_Radius;
			const float height = scale.y * m_Height;
			m_Shape->SetHeightAndRadius(height, radius);

			if (IsObstacle())
				CreateObstacle(); // Recreate obstacle
		}
	}

	MeshColliderComponent& MeshColliderComponent::operator=(const MeshColliderComponent& other)
	{
		BaseColliderComponent::operator=(other);

		// This call is disabled since `SetIsConvex` will call it anyway. So we just set the mesh
		// SetCollisionMeshAsset(other.m_CollisionMeshAsset);
		m_CollisionMeshAsset = other.m_CollisionMeshAsset;
		
		{
			// Should be in this order so that we don't need to call `SetIsTwoSided`
			bTwoSided = other.bTwoSided;
			SetIsConvex(other.bConvex);
		}

		UpdatePhysicsTransform();

		return *this;
	}

	void MeshColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetIsTrigger(bTrigger);
	}
	
	void MeshColliderComponent::UpdatePhysicsMaterials()
	{
		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetPhysicsMaterial(m_MaterialAsset);
	}

	void MeshColliderComponent::CreateObstacle()
	{
		// When/If support is added, update `Scene::BuildNavMesh()`
		EG_CORE_ASSERT(false);
		EG_CORE_ERROR("MeshColliderComponent can't be an obstacle!");
	}

	void MeshColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		if (m_Shapes[0]) // No need to enable it for the backside
			m_Shapes[0]->SetShowCollision(bShowCollision);
	}
	
	void MeshColliderComponent::SetCollisionMeshAsset(const Ref<AssetStaticMesh>& meshAsset)
	{
		m_CollisionMeshAsset = meshAsset;

		auto actor = Parent.GetPhysicsActor();
		if (actor)
		{
			for (auto& shape : m_Shapes)
			{
				actor->RemoveCollider(shape);
				shape.reset();
			}

			if (m_CollisionMeshAsset)
				m_Shapes = actor->AddCollider(*this);
		}
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			if (m_CollisionMeshAsset)
				m_Shapes = actor->AddCollider(*this);
			else
			{
				for (auto& shape : m_Shapes)
					shape.reset();
			}
		}

		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetFilterData(actor->GetFilterData());

		SetShowCollision(bShowCollision);
		SetIsTrigger(bTrigger);
	}
	
	void MeshColliderComponent::OnInit(Entity entity)
	{
		if (Parent && Parent.HasComponent<StaticMeshComponent>())
		{
			auto& comp = Parent.GetComponent<StaticMeshComponent>();
			m_CollisionMeshAsset = comp.GetMeshAsset();
			SetRelativeTransform(comp.GetRelativeTransform());
		}
		
		SetCollisionMeshAsset(m_CollisionMeshAsset);
	}

	void MeshColliderComponent::OnRemoved(Entity entity)
	{
		BaseColliderComponent::OnRemoved(entity);
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			for (auto& shape : m_Shapes)
				if (shape)
				{
					actor->RemoveCollider(shape);
					shape.reset();
				}	
		}
	}
	
	void MeshColliderComponent::UpdatePhysicsTransform()
	{
		for (auto& shape : m_Shapes)
		{
			if (shape)
			{
				shape->SetRelativeLocationAndRotation(RelativeTransform);
				shape->SetScale(WorldTransform.Scale3D);
			}
		}
	}
	
	SkeletalMeshComponent::~SkeletalMeshComponent()
	{
		if (m_MeshAsset)
			m_MeshAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
		if (m_AnimGraphAsset)
			m_AnimGraphAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
	}

	SkeletalMeshComponent& SkeletalMeshComponent::operator=(const SkeletalMeshComponent& other)
	{
		if (this == &other)
			return *this;

		SceneComponent::operator=(other);

		m_MeshAsset = other.m_MeshAsset;
		m_MaterialAssets = other.m_MaterialAssets;
		m_AnimAsset = other.m_AnimAsset;
		m_AnimGraphAsset = other.m_AnimGraphAsset;
		if (other.m_Graph)
		{
			VariablesMap copiedVars;
			for (const auto& [name, var] : other.m_Graph->GetVariables())
				copiedVars[name] = CopyVarByType(var);

			m_Graph = MakeRef<AnimationGraph>(other.m_Graph); // Copy
			m_Graph->SetVariablesToUse(copiedVars); // Forcing all graphs/subgraphs to use these variables
		}
		m_bCastsShadows = other.m_bCastsShadows;
		m_bReceivesDecals = other.m_bReceivesDecals;
		CurrentClipPlayTime = other.CurrentClipPlayTime;
		PrevClipPlayTime = other.PrevClipPlayTime;
		ClipPlaybackSpeed = other.ClipPlaybackSpeed;
		PrevClipPlaybackSpeed = other.PrevClipPlaybackSpeed;
		bClipLooping = other.bClipLooping;
		AnimType = other.AnimType;

		if (m_MeshAsset)
		{
			if (other.m_bRagdollEnabled)
			{
				SetRagdollEnabled(true);
			}
		}
		else
		{
			m_bRagdollEnabled = other.m_bRagdollEnabled;
		}

		Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
		return *this;
	}

	void SkeletalMeshComponent::SetMeshAsset(const Ref<AssetSkeletalMesh>& mesh)
	{
		const bool bHadValidAsset = m_MeshAsset.operator bool();

		if (bHadValidAsset)
			m_MeshAsset->RemoveOnAssetModifiedCallback(m_CallbackID);

		m_MeshAsset = mesh;
		CurrentClipPlayTime = 0.f;
		PrevClipPlayTime = 0.f;

		if (m_MeshAsset)
		{
			const auto& mesh = m_MeshAsset->GetMesh();
			const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
			m_MaterialAssets.resize(materialsCount);
			for (uint32_t i = 0; i < materialsCount; ++i)
				m_MaterialAssets[i] = mesh->GetMaterialAsset(i);
		}
		else
		{
			m_MaterialAssets.clear();
		}

		if (bHadValidAsset)
		{
			if (IsRagdollEnabled())
			{
				SetRagdollEnabled(false);
				// Restore the state after `SetRagdollEnabled()` call.
				// Needed so that we can automatically create a ragdoll when `SetMeshAsset` is called next time
				m_bRagdollEnabled = true;
			}
		}
		if (m_MeshAsset)
		{
			m_MeshAsset->AddOnAssetModifiedCallback(m_CallbackID, [entity = Parent]() mutable
				{
					auto& component = entity.GetComponent<SkeletalMeshComponent>();
					if (component.IsRagdollEnabled())
					{
						// Recreate ragdoll
						component.SetRagdollEnabled(false);
						component.SetRagdollEnabled(true);
					}
				}
			);

			if (IsRagdollEnabled())
			{
				// Required, otherwise `SetRagdollEnabled(true)` will early-exit
				m_bRagdollEnabled = false;
				SetRagdollEnabled(true);
			}
		}

		Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
	}
	
	void SkeletalMeshComponent::SetAnimationGraphAsset(const Ref<AssetAnimationGraph>& anim)
	{
		const bool bSameGraph = m_AnimGraphAsset == anim;
		if (bSameGraph == false)
		{
			if (m_AnimGraphAsset)
			{
				m_AnimGraphAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
			}
		}
		m_AnimGraphAsset = anim;
		if (m_AnimGraphAsset)
		{
			if (bSameGraph == false)
			{
				m_AnimGraphAsset->AddOnAssetModifiedCallback(m_CallbackID, [entity = Parent, animGraph = m_AnimGraphAsset]() mutable
				{
					entity.GetComponent<SkeletalMeshComponent>().SetAnimationGraphAsset(animGraph); // Update graph
				});
			}
			// Merging means that the values of old variables will be used if possible
			const bool bMergeVars = bSameGraph && m_Graph;

			VariablesMap oldVars;
			if (bMergeVars)
				oldVars = m_Graph->GetVariables();

			{
				VariablesMap copiedVars;
				for (const auto& [name, var] : m_AnimGraphAsset->GetGraph()->GetVariables())
					copiedVars[name] = CopyVarByType(var);

				m_Graph = MakeRef<AnimationGraph>(m_AnimGraphAsset->GetGraph()); // Copy
				m_Graph->SetVariablesToUse(copiedVars); // Forcing all graphs/subgraphs to use these variables
			}
			if (bMergeVars)
			{
				const auto& usedVars = m_Graph->GetVariables();
				for (const auto& [name, oldVar] : oldVars)
				{
					auto it = usedVars.find(name);
					if (it == usedVars.end())
						continue; // Var is not present in the newly compiled graph. Ignore it

					// If types match, copy the old variable's value
					// Otherwise, keep newly compiled variable
					auto& usedVar = it->second;
					if (usedVar->GetType() == oldVar->GetType())
						usedVar->CopyValue(oldVar);
				}
			}
		}
		else
			m_Graph.reset();
	}

	void SkeletalMeshComponent::SetWorldTransform(const Transform& worldTransform)
	{
		SceneComponent::SetWorldTransform(worldTransform);
		Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnTransformChanged);
	}

	void SkeletalMeshComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		SceneComponent::SetRelativeTransform(relativeTransform);
		Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnTransformChanged);
	}

	Transform SkeletalMeshComponent::GetBoneWorldTransform(const std::string_view boneName)
	{
		const auto& asset = GetMeshAsset();
		if (!asset)
			return {};

		Transform result;
		Utils::GetBoneWorldTransform(LastPose, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, IsRagdollEnabled(), Math::ToTransformMatrix(GetWorldTransform()), boneName, &result);
		return result;
	}

	glm::vec3 SkeletalMeshComponent::GetBoneWorldLocation(const std::string_view boneName)
	{
		return GetBoneWorldTransform(boneName).Location;
	}

	Rotator SkeletalMeshComponent::GetBoneWorldRotation(const std::string_view boneName)
	{
		return GetBoneWorldTransform(boneName).Rotation;
	}

	glm::vec3 SkeletalMeshComponent::GetBoneWorldScale(const std::string_view boneName)
	{
		return GetBoneWorldTransform(boneName).Scale3D;
	}

	void SkeletalMeshComponent::TriggerAnimationEvent(const std::string& name)
	{
		if (Parent.HasComponent<ScriptComponent>() == false)
			return;

		if (ScriptEngine::ModuleExists(Parent.GetComponent<ScriptComponent>().ModuleName))
			ScriptEngine::OnAnimationEventEntity(Parent, name);
	}

	void SkeletalMeshComponent::SetRagdollEnabled(bool bEnabled)
	{
		if (bEnabled == m_bRagdollEnabled)
			return;

		if (bEnabled)
		{
			m_RagdollActor = Parent.GetScene()->GetPhysicsScene()->CreateRagdoll(*this);
			m_bRagdollEnabled = m_RagdollActor.operator bool();
		}
		else
		{
			Parent.GetScene()->GetPhysicsScene()->ReleaseRagdoll(*this);
			m_RagdollActor.reset();
			m_bRagdollEnabled = bEnabled;
		}
	}

	bool SkeletalMeshComponent::IsRagdollCollisionShown() const
	{
		if (m_RagdollActor)
			return m_RagdollActor->IsCollisionShown();
		
		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetShowRagdollCollision. Ragdoll is null");
		return false;
	}

	void SkeletalMeshComponent::SetShowRagdollCollision(bool bShow)
	{
		if (m_RagdollActor)
			m_RagdollActor->SetShowCollision(bShow);
		else
			EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetShowRagdollCollision. Ragdoll is null");
	}

	Transform SkeletalMeshComponent::GetRagdollBoneWorldTransform(const std::string& name) const
	{
		if (m_RagdollActor)
			return m_RagdollActor->GetBoneWorldTransform(name);

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::GetRagdollBoneWorldTransform. Ragdoll is null");
		return {};
	}

	ParticleSystemComponent::~ParticleSystemComponent()
	{
		if (m_Asset)
			m_Asset->RemoveOnAssetModifiedCallback(m_SystemID);
	}

	ParticleSystemComponent& ParticleSystemComponent::operator=(const ParticleSystemComponent& other)
	{
		if (this == &other)
			return *this;

		SceneComponent::operator=(other);

		bAutospawn = other.bAutospawn;
		SetAsset(other.m_Asset);

		return *this;
	}

	void ParticleSystemComponent::SetAsset(const Ref<AssetParticleSystem>& asset)
	{
		if (m_Asset == asset)
			return;

		const bool bHadValidAsset = m_Asset.operator bool();
		if (bHadValidAsset)
			m_Asset->RemoveOnAssetModifiedCallback(m_SystemID);

		const bool bWillUseNewAsset = asset && bAutospawn;
		if (!bWillUseNewAsset) // Destroy the old one if we're not going to spawn a new one
			Destroy();

		m_Asset = asset;
		if (bWillUseNewAsset)
		{
			if (bHadValidAsset && bSpawned)
			{
				m_Asset->AddOnAssetModifiedCallback(m_SystemID, [entity = Parent]() mutable
				{
					entity.GetComponent<ParticleSystemComponent>().Update();
				});
				Update();
			}
			else
				Spawn();
		}
	}
	
	void ParticleSystemComponent::Spawn()
	{
		if (!bSpawned && m_Asset)
		{
			Parent.GetScene()->AddParticleSystem(this);
			bSpawned = true;
			m_Asset->AddOnAssetModifiedCallback(m_SystemID, [entity = Parent]() mutable
			{
				entity.GetComponent<ParticleSystemComponent>().Update();
			});
		}
	}

	void ParticleSystemComponent::Destroy()
	{
		if (bSpawned)
		{
			Parent.GetScene()->RemoveParticleSystem(this);
			bSpawned = false;
			m_Asset->RemoveOnAssetModifiedCallback(m_SystemID);
		}
	}
	
	void ParticleSystemComponent::Update()
	{
		if (bSpawned)
			Parent.GetScene()->UpdateParticleSystem(this);
	}
	
	NavigationMeshComponent& NavigationMeshComponent::operator=(const NavigationMeshComponent& other)
	{
		if (this == &other)
			return *this;

		SceneComponent::operator=(other);
		bAutoRebuild = other.bAutoRebuild;
		m_Settings = other.m_Settings;
		if (other.m_NavMesh)
		{
			Parent.GetScene()->BuildNavMesh(this);
		}

		return *this;
	}

	void NavigationMeshComponent::GetNavMeshDebugDraw(duDebugDraw* debugDraw) const
	{
		if (!m_NavMesh)
			return;

		m_NavMesh->GetDebugDraw(debugDraw);
	}

	void NavigationMeshComponent::Update(Timestep ts)
	{
		if (!m_NavMesh)
			return;

		m_NavMesh->Update(ts);
	}

	void NavigationMeshComponent::Build()
	{
		auto& physicsScene = Parent.GetScene()->GetPhysicsScene();

		glm::mat4 worldTr = Math::ToTransformMatrix(GetWorldTransform());
		auto settings = m_Settings;
		settings.AABB.Min = worldTr * glm::vec4(m_Settings.AABB.Min, 1.f);
		settings.AABB.Max = worldTr * glm::vec4(m_Settings.AABB.Max, 1.f);

		const QueryHits overlaps = physicsScene->CollectCollidersWithinVolume(settings.AABB);
		
		// Remove colliders that shouldn't affect the nav mesh
		QueryHits filteredOverlaps;
		filteredOverlaps.reserve(overlaps.size());
		for (const auto& overlap : overlaps)
		{
			if (!overlap.Shape)
				continue;

			const BaseColliderComponent* collider = nullptr;

			switch (overlap.Shape->GetType())
			{
			case ColliderType::Box:
				EG_CORE_ASSERT(overlap.EntityID.HasComponent<BoxColliderComponent>());
				collider = &overlap.EntityID.GetComponent<BoxColliderComponent>();
				break;
			case ColliderType::Sphere:
				EG_CORE_ASSERT(overlap.EntityID.HasComponent<SphereColliderComponent>());
				collider = &overlap.EntityID.GetComponent<SphereColliderComponent>();
				break;
			case ColliderType::Capsule:
				EG_CORE_ASSERT(overlap.EntityID.HasComponent<CapsuleColliderComponent>());
				collider = &overlap.EntityID.GetComponent<CapsuleColliderComponent>();
				break;
			case ColliderType::ConvexMesh:
			case ColliderType::TriangleMesh:
				EG_CORE_ASSERT(overlap.EntityID.HasComponent<MeshColliderComponent>());
				collider = &overlap.EntityID.GetComponent<MeshColliderComponent>();
				break;
			}

			EG_CORE_ASSERT(collider);
			if (collider->DoesAffectNavMeshBuild())
			{
				filteredOverlaps.push_back(overlap);
			}
		}

		auto geometry = physicsScene->AppendColliderGeometry(settings.AABB, filteredOverlaps);
		m_NavMesh = AINavigation::Mesh::Create(geometry, settings, m_CrowdSettings);
	}
	
	NavigationCrowdAgentComponent& NavigationCrowdAgentComponent::operator=(const NavigationCrowdAgentComponent& other)
	{
		if (this == &other)
			return *this;

		Component::operator=(other);
		m_Settings = other.m_Settings;
		if (other.IsValid())
			CreateAgent(Parent.GetWorldLocation());

		return *this;
	}
	
	void NavigationCrowdAgentComponent::TeleportAgent(const glm::vec3& location)
	{
		if (!IsValid())
			return;

		RemoveAgent();
		CreateAgent(location);
	}

	void NavigationCrowdAgentComponent::SetMoveTarget(const glm::vec3& pos)
	{
		if (!IsValid())
			return;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		navMesh->GetCrowd().SetMoveTarget(m_AgentIndex, pos);
	}

	void NavigationCrowdAgentComponent::ResetMoveTarget()
	{
		if (!IsValid())
			return;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		navMesh->GetCrowd().ResetMoveTarget(m_AgentIndex);
	}

	bool NavigationCrowdAgentComponent::GetLocation(glm::vec3* outLocation) const
	{
		if (!IsValid())
			return false;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return false;

		return navMesh->GetCrowd().GetAgentLocation(m_AgentIndex, outLocation);
	}

	bool NavigationCrowdAgentComponent::GetVelocity(glm::vec3* outVelocity) const
	{
		if (!IsValid())
			return false;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return false;

		return navMesh->GetCrowd().GetAgentVelocity(m_AgentIndex, outVelocity);
	}

	MoveRequestState NavigationCrowdAgentComponent::GetAgentTargetState() const
	{
		if (!IsValid())
			return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return MoveRequestState::DT_CROWDAGENT_TARGET_NONE;

		return navMesh->GetCrowd().GetAgentTargetState(m_AgentIndex);
	}
	
	void NavigationCrowdAgentComponent::SetSettings(const AINavigation::AgentSettings& settings)
	{
		m_Settings = settings;

		if (!IsValid())
			return;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		navMesh->GetCrowd().UpdateAgentSettings(m_AgentIndex, m_Settings);
	}
	
	void NavigationCrowdAgentComponent::CreateAgent(const glm::vec3& location)
	{
		if (IsValid())
			return;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		m_AgentIndex = navMesh->GetCrowd().AddAgent(location, m_Settings);
	}
	
	void NavigationCrowdAgentComponent::RemoveAgent()
	{
		if (!IsValid())
			return;

		const auto& navMesh = Parent.GetScene()->GetNavMesh();
		if (!navMesh)
			return;

		navMesh->GetCrowd().RemoveAgent(m_AgentIndex);
		m_AgentIndex = -1;
	}
}
