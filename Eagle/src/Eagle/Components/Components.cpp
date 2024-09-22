#include "egpch.h"
#include "Component.h"

#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsShapes.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Components.h"

namespace Eagle
{
	namespace Utils
	{
		// True if found
		static bool GetBoneWorldTransform(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const std::string_view targetBoneName, Transform* outTransform)
		{
			const std::string& nodeName = node.Name;
			glm::mat4 globalTransformation;
			if (auto it = pose.Bones.find(nodeName); it != pose.Bones.end())
			{
				const auto& bone = it->second;
				globalTransformation = parentTransform * Math::ToTransformMatrix(bone);
			}
			else
				globalTransformation = parentTransform * node.Transformation;

			if (nodeName == targetBoneName)
			{
				*outTransform = Math::DecomposeTransformMatrix(globalTransformation);
				return true;
			}

			for (auto& child : node.Children)
				if (GetBoneWorldTransform(pose, child, globalTransformation, targetBoneName, outTransform))
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
	
	void BoxColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void BoxColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset ? m_MaterialAsset->GetMaterial() : PhysicsMaterial{});
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
		m_Shape->SetSize(m_Size);
	}

	void BoxColliderComponent::UpdatePhysicsTransform()
	{
		if (m_Shape)
		{
			m_Shape->SetRelativeLocationAndRotation(RelativeTransform);

			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			const glm::vec3 newSize = WorldTransform.Scale3D * m_Size;
			if (newSize != oldSize)
				m_Shape->SetSize(m_Size);
		}
	}

	void SphereColliderComponent::SetRadius(float radius)
	{
		Radius = glm::max(radius, 0.f);
		m_Shape->SetRadius(Radius);
	}
	
	void SphereColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void SphereColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset ? m_MaterialAsset->GetMaterial() : PhysicsMaterial{});
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

			const glm::vec3& newSize = WorldTransform.Scale3D;
			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			if (newSize != oldSize)
				m_Shape->SetRadius(Radius);
		}
	}

	void CapsuleColliderComponent::SetIsTrigger(bool bTrigger)
	{
		this->bTrigger = bTrigger;
		m_Shape->SetIsTrigger(bTrigger);
	}
	
	void CapsuleColliderComponent::UpdatePhysicsMaterials()
	{
		m_Shape->SetPhysicsMaterial(m_MaterialAsset ? m_MaterialAsset->GetMaterial() : PhysicsMaterial{});
	}

	void CapsuleColliderComponent::SetShowCollision(bool bShowCollision)
	{
		this->bShowCollision = bShowCollision;
		m_Shape->SetShowCollision(bShowCollision);
	}
	
	void CapsuleColliderComponent::SetHeightAndRadius(float height, float radius)
	{
		Height = glm::max(height, 0.f);
		Radius = glm::max(radius, 0.f);
		m_Shape->SetHeightAndRadius(Height, Radius);
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

			const glm::vec3& newSize = WorldTransform.Scale3D;
			const glm::vec3& oldSize = m_Shape->GetColliderScale();
			if (newSize != oldSize)
				m_Shape->SetHeightAndRadius(Height, Radius);
		}
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
		PhysicsMaterial material = m_MaterialAsset ? m_MaterialAsset->GetMaterial() : PhysicsMaterial{};
		for (auto& shape : m_Shapes)
			if (shape)
				shape->SetPhysicsMaterial(material);
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

				const glm::vec3& newSize = WorldTransform.Scale3D;
				const glm::vec3& oldSize = shape->GetColliderScale();
				if (newSize != oldSize)
					shape->SetScale(newSize);
			}
		}
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

		Parent.SignalComponentChanged<SkeletalMeshComponent>(Notification::OnStateChanged);
		return *this;
	}
	
	void SkeletalMeshComponent::SetAnimationGraphAsset(const Ref<AssetAnimationGraph>& anim)
	{
		const bool bSameGraph = m_AnimGraphAsset == anim;
		m_AnimGraphAsset = anim;
		if (m_AnimGraphAsset)
		{
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

	Transform SkeletalMeshComponent::GetBoneWorldTransform(const std::string_view boneName)
	{
		const auto& asset = GetMeshAsset();
		if (!asset)
			return {};

		Transform result;
		Utils::GetBoneWorldTransform(LastPose, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, Math::ToTransformMatrix(GetWorldTransform()), boneName, &result);
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

	ParticleSystemComponent::ParticleSystemComponent(const Entity& entity, const Ref<AssetParticleSystem>& asset)
		: SceneComponent(entity), m_Asset(asset)
	{
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
				m_Asset->AddOnAssetModifiedCallback(m_SystemID, [this]() { Update(); });
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
			m_Asset->AddOnAssetModifiedCallback(m_SystemID, [this]() { Update(); });
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
}
