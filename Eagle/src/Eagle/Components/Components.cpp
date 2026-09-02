#include "egpch.h"
#include "Component.h"

#include "Eagle/Animation/AnimationSystem.h"
#include "Eagle/Physics/PhysicsActor.h"
#include "Eagle/Physics/PhysicsRagdollActor.h"
#include "Eagle/Physics/PhysicsShapes.h"
#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/Physics/PhysicsCharacterController.h"
#include "Components.h"

namespace Eagle
{
	namespace Utils
	{
		// True if found
		static bool GetBoneWorldTransform(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& parentTransform, const std::string_view targetBoneName, Transform* outTransform)
		{
			glm::mat4 globalTransformation;
			if (auto it = pose.FindBone(node.GetNameHash()); it != pose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				globalTransformation = parentTransform * boneTransform;
			}
			else
				globalTransformation = parentTransform * node.Transformation;

			if (node.GetName() == targetBoneName)
			{
				*outTransform = Math::DecomposeTransformMatrix(globalTransformation);
				return true;
			}

			for (auto& child : node.Children)
				if (GetBoneWorldTransform(pose, child, globalTransformation, targetBoneName, outTransform))
					return true;

			return false;
		}

		static bool GetBoneWorldTransform_Ragdoll(const SkeletalPose& pose, const BoneNode& node, const glm::mat4& worldTransform, const glm::mat4& parentTransform, const std::string_view targetBoneName, Transform* outTransform)
		{
			glm::mat4 globalTransformation;
			if (auto it = pose.FindBone(node.GetNameHash()); it != pose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				// If a ragdoll, then it's already a global transform
				globalTransformation = boneTransform;
			}
			else
				globalTransformation = parentTransform * node.Transformation;

			if (node.GetName() == targetBoneName)
			{
				*outTransform = Math::DecomposeTransformMatrix(worldTransform * globalTransformation);
				return true;
			}

			for (auto& child : node.Children)
				if (GetBoneWorldTransform_Ragdoll(pose, child, worldTransform, globalTransformation, targetBoneName, outTransform))
					return true;

			return false;
		}

		// True if found
		static bool HasBone(const BoneNode& node, const std::string_view targetBoneName)
		{
			const std::string& nodeName = node.GetName();
			if (nodeName == targetBoneName)
			{
				return true;
			}

			for (const auto& child : node.Children)
			{
				if (HasBone(child, targetBoneName))
					return true;
			}

			return false;
		}
	}

	void RigidBodyComponent::SetBodyType(PhysicsBodyType type)
	{
		m_BodyType = type;

		if (const auto& actor = Parent.GetPhysicsActor())
		{
			if (Parent.HasComponent<BoxColliderComponent>())
				Parent.GetComponent<BoxColliderComponent>().OnRemoved();
			if (Parent.HasComponent<SphereColliderComponent>())
				Parent.GetComponent<SphereColliderComponent>().OnRemoved();
			if (Parent.HasComponent<CapsuleColliderComponent>())
				Parent.GetComponent<CapsuleColliderComponent>().OnRemoved();
			if (Parent.HasComponent<MeshColliderComponent>())
				Parent.GetComponent<MeshColliderComponent>().OnRemoved();

			Parent.GetScene()->GetPhysicsScene()->RemovePhysicsActor(actor);
			
			// Recreate actor and colliders
			Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			if (Parent.HasComponent<BoxColliderComponent>())
				Parent.GetComponent<BoxColliderComponent>().OnInit();
			if (Parent.HasComponent<SphereColliderComponent>())
				Parent.GetComponent<SphereColliderComponent>().OnInit();
			if (Parent.HasComponent<CapsuleColliderComponent>())
				Parent.GetComponent<CapsuleColliderComponent>().OnInit();
			if (Parent.HasComponent<MeshColliderComponent>())
				Parent.GetComponent<MeshColliderComponent>().OnInit();

			Parent.GetScene()->RebuildNavMesh();
		}
	}

	void RigidBodyComponent::SetCollisionDetectionType(CollisionDetectionType type)
	{
		m_CollisionDetection = type;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetCollisionDetectionType(m_CollisionDetection);
	}

	void RigidBodyComponent::SetPositionSolverIterations(uint32_t iterations)
	{
		PositionSolverIterations = std::clamp(iterations, PhysicsSettings::MinPositionSolverIterations, PhysicsSettings::MaxPositionSolverIterations);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetPositionSolverIterations(PositionSolverIterations);
	}

	void RigidBodyComponent::SetVelocitySolverIterations(uint32_t iterations)
	{
		VelocitySolverIterations = std::clamp(iterations, PhysicsSettings::MinVelocitySolverIterations, PhysicsSettings::MaxVelocitySolverIterations);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetVelocitySolverIterations(VelocitySolverIterations);
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

	void RigidBodyComponent::WakeUp()
	{
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->WakeUp();
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
		SetFlag(m_LockFlags, flag, value);
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
	}

	void RigidBodyComponent::SetLockFlag(ActorLockFlag flag)
	{
		m_LockFlags = flag;
		if (const auto& actor = Parent.GetPhysicsActor())
			actor->SetLockFlag(m_LockFlags);
	}

	CharacterControllerComponent::CharacterControllerComponent(const Entity& entity)
		: SceneComponent(entity)
	{
		m_Controller = MakeRef<PhysicsCharacterController>(*this);
	}

	CharacterControllerComponent& CharacterControllerComponent::operator=(const CharacterControllerComponent& other)
	{
		if (this == &other)
			return *this;

		SetSlopeLimit(other.GetSlopeLimit());
		SetContactOffset(other.GetContactOffset());
		SetStepOffset(other.GetStepOffset());
		SetPhysicsMaterialAsset(other.GetPhysicsMaterialAsset());
		SetShapeType(other.GetShapeType());
		SetCapsuleRadius(other.GetCapsuleRadius());
		SetCapsuleHeight(other.GetCapsuleHeight());
		SetBoxSize(other.GetBoxSize());
		SetCapsuleClimbingMode(other.GetCapsuleClimbingMode());
		SetCollisionGroup(other.GetCollisionGroup());
		SetInteractingCollisionGroup(other.GetInteractingCollisionGroup());
		SetShowCollision(other.IsCollisionVisible());
		SetDoesCollideWithOtherControllers(other.DoesCollideWithOtherControllers());
		SetControllerUpDirection(other.GetControllerUpDirection());
		bMoveWholeEntity = other.bMoveWholeEntity;
		bUseFootLocation = other.bUseFootLocation;

		// Needs to be last because other function calls can adjust the location,
		// but we want it to match exactly
		SceneComponent::operator=(other);
		UpdateTransform(false);

		return *this;
	}

	CharacterControllerCollisionFlags CharacterControllerComponent::Move(const glm::vec3& disp, float minDist, float elapsedTime)
	{
		bCurrentlyMoving = true;
		auto result = m_Controller->Move(disp, minDist, elapsedTime);

		const glm::vec3 location = bUseFootLocation ? m_Controller->GetFootWorldLocation() : m_Controller->GetWorldLocation();
		if (bMoveWholeEntity)
		{
			Parent.SetWorldLocation(location - RelativeTransform.Location);
		}
		else
		{
			Transform tr = GetWorldTransform();
			tr.Location = location;
			SetWorldTransform(tr);
		}
		bCurrentlyMoving = false;

		return result;
	}

	void CharacterControllerComponent::SetSlopeLimit(float degrees)
	{
		m_Controller->SetSlopeLimit(degrees);
	}

	float CharacterControllerComponent::GetSlopeLimit() const
	{
		return m_Controller->GetSlopeLimit();
	}

	void CharacterControllerComponent::SetContactOffset(float contactOffset)
	{
		m_Controller->SetContactOffset(contactOffset);
	}

	float CharacterControllerComponent::GetContactOffset() const
	{
		return m_Controller->GetContactOffset();
	}

	void CharacterControllerComponent::SetStepOffset(float stepOffset)
	{
		m_Controller->SetStepOffset(stepOffset);
	}

	float CharacterControllerComponent::GetStepOffset() const
	{
		return m_Controller->GetStepOffset();
	}

	void CharacterControllerComponent::SetPhysicsMaterialAsset(const Ref<AssetPhysicsMaterial>& material)
	{
		m_Controller->SetPhysicsMaterialAsset(material);
	}

	const Ref<AssetPhysicsMaterial>& CharacterControllerComponent::GetPhysicsMaterialAsset() const
	{
		return m_Controller->GetPhysicsMaterialAsset();
	}

	void CharacterControllerComponent::SetShapeType(CharacterControllerShape shape)
	{
		m_Controller->SetShapeType(shape);
	}

	CharacterControllerShape CharacterControllerComponent::GetShapeType() const
	{
		return m_Controller->GetShapeType();
	}

	void CharacterControllerComponent::SetCapsuleClimbingMode(CapsuleClimbingMode mode)
	{
		m_Controller->SetCapsuleClimbingMode(mode);
	}

	CapsuleClimbingMode CharacterControllerComponent::GetCapsuleClimbingMode() const
	{
		return m_Controller->GetCapsuleClimbingMode();
	}

	void CharacterControllerComponent::SetCapsuleRadius(float radius)
	{
		m_Radius = radius;
		m_Controller->SetCapsuleRadius(GetScaledCapsuleRadius());
	}

	void CharacterControllerComponent::SetCapsuleHeight(float height)
	{
		m_Height = height;
		m_Controller->SetCapsuleHeight(GetScaledCapsuleHeight());

		// Controller can change the position, update it
		Transform transform = WorldTransform;
		transform.Location = m_Controller->GetWorldLocation();
		SetWorldTransform(transform);
	}

	void CharacterControllerComponent::SetBoxSize(const glm::vec3& size)
	{
		m_Size = size;
		m_Controller->SetBoxHalfExtent(GetScaledBoxSize() * 0.5f);

		// Controller can change the position, update it
		Transform transform = WorldTransform;
		transform.Location = m_Controller->GetWorldLocation();
		SetWorldTransform(transform);
	}

	void CharacterControllerComponent::SetWorldTransform(const Transform& worldTransform)
	{
		// Rotation is not supported.
		Transform tr = worldTransform;
		tr.Rotation = Rotator{};

		const bool bTransformChanged = tr != WorldTransform;

		// Same as SceneComponent::SetWorldTransform, but it ignores rotations
		{
			const auto& parentWorldTransform = Parent.GetWorldTransform();
			WorldTransform = tr;

			RelativeTransform.Location = WorldTransform.Location - parentWorldTransform.Location;
			RelativeTransform.Scale3D = WorldTransform.Scale3D / parentWorldTransform.Scale3D;

			RelativeTransform.Location /= parentWorldTransform.Scale3D; // Undo parent's scaling
		}

		if (bTransformChanged)
			UpdateTransform(bUseFootLocation);
	}

	void CharacterControllerComponent::SetRelativeTransform(const Transform& relativeTransform)
	{
		// Rotation is not supported.
		Transform tr = relativeTransform;
		tr.Rotation = Rotator{};

		const Transform trBefore = WorldTransform.Location;

		glm::vec3 rotated;
		// Same as SceneComponent::SetRelativeTransform, but it ignores rotations
		{
			const auto& parentWorldTransform = Parent.GetWorldTransform();
			RelativeTransform = tr;

			WorldTransform.Scale3D = parentWorldTransform.Scale3D * RelativeTransform.Scale3D;

			rotated = (RelativeTransform.Location * parentWorldTransform.Scale3D);
			WorldTransform.Location = parentWorldTransform.Location + rotated;
		}
		const bool bTransformChanged = trBefore != WorldTransform;

		if (bTransformChanged)
		{
			UpdateTransform(bUseFootLocation);
		}
	}

	glm::vec3 CharacterControllerComponent::GetControllerWorldLocation() const
	{
		return m_Controller->GetWorldLocation();
	}

	glm::vec3 CharacterControllerComponent::GetControllerFootWorldLocation() const
	{
		return m_Controller->GetFootWorldLocation();
	}

	void CharacterControllerComponent::UpdateTransform(bool bUseFoot)
	{
		// Hacky way to prevent controller affecting itself
		if (bCurrentlyMoving)
			return;

		m_Controller->SetBoxHalfExtent(GetScaledBoxSize() * 0.5f);
		m_Controller->SetCapsuleRadius(GetScaledCapsuleRadius());
		m_Controller->SetCapsuleHeight(GetScaledCapsuleHeight());
		if (bUseFoot)
		{
			m_Controller->SetFootWorldLocation(WorldTransform.Location);
		}
		else
		{
			m_Controller->SetWorldLocation(WorldTransform.Location);
		}
	}

	void CharacterControllerComponent::SetCollisionGroup(CollisionGroup groups)
	{
		m_Controller->SetCollisionGroup(groups);
	}

	CollisionGroup CharacterControllerComponent::GetCollisionGroup() const
	{
		return m_Controller->GetCollisionGroup();
	}

	void CharacterControllerComponent::SetInteractingCollisionGroup(CollisionGroup groups)
	{
		m_Controller->SetInteractingCollisionGroup(groups);
	}

	CollisionGroup CharacterControllerComponent::GetInteractingCollisionGroup() const
	{
		return m_Controller->GetInteractingCollisionGroup();
	}

	void CharacterControllerComponent::SetDoesCollideWithOtherControllers(bool bCollides)
	{
		m_Controller->SetDoesCollideWithOtherControllers(bCollides);
	}

	bool CharacterControllerComponent::DoesCollideWithOtherControllers() const
	{
		return m_Controller->DoesCollideWithOtherControllers();
	}

	glm::vec3 CharacterControllerComponent::GetScaledBoxSize() const
	{
		return WorldTransform.Scale3D * m_Size;
	}

	float CharacterControllerComponent::GetScaledCapsuleRadius() const
	{
		const auto& scale = WorldTransform.Scale3D;
		return glm::max(scale.x, scale.z) * m_Radius;
	}

	float CharacterControllerComponent::GetScaledCapsuleHeight() const
	{
		const auto& scale = WorldTransform.Scale3D;
		return scale.y * m_Height;
	}

	void CharacterControllerComponent::SetControllerUpDirection(const glm::vec3& upDir)
	{
		m_Controller->SetUpDirection(upDir);
	}

	const glm::vec3& CharacterControllerComponent::GetControllerUpDirection() const
	{
		return m_Controller->GetUpDirection();
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
		if (this == &other)
			return *this;

		SceneComponent::operator=(other);
		SetPhysicsMaterialAsset(other.m_MaterialAsset);
		SetIsTrigger(other.bTrigger);
		SetShowCollision(other.bShowCollision);
		SetAffectsNavMeshBuild(other.bAffectsNavMeshBuild);
		SetIsObstacle(other.IsObstacle());
		SetCollisionGroup(other.m_CollisionGroup);
		SetInteractingCollisionGroup(other.m_InteractingCollisionGroup);
		SetCollisionEnabled(other.IsCollisionEnabled());

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
		if (this == &other)
			return *this;

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

	void BoxColliderComponent::UpdateShowCollisionState()
	{
		m_Shape->SetShowCollision(bShowCollision || Parent.GetScene()->IsForcingShowCollision());
	}
	
	void BoxColliderComponent::OnInit()
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
	}

	void BoxColliderComponent::OnRemoved()
	{
		BaseColliderComponent::OnRemoved();
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			actor->RemoveCollider(m_Shape);
			m_Shape.reset();
		}
	}

	void BoxColliderComponent::SetCollisionGroup(CollisionGroup group)
	{
		m_CollisionGroup = group;
		m_Shape->SetCollisionGroup(m_CollisionGroup);
	}

	void BoxColliderComponent::SetInteractingCollisionGroup(CollisionGroup group)
	{
		m_InteractingCollisionGroup = group;
		m_Shape->SetInteractingCollisionGroup(m_InteractingCollisionGroup);
	}

	void BoxColliderComponent::SetCollisionEnabled(bool bEnabled)
	{
		bCollisionEnabled = bEnabled;
		m_Shape->SetCollisionEnabled(bEnabled);
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
		if (this == &other)
			return *this;

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

	void SphereColliderComponent::UpdateShowCollisionState()
	{
		m_Shape->SetShowCollision(bShowCollision || Parent.GetScene()->IsForcingShowCollision());
	}

	void SphereColliderComponent::SetCollisionGroup(CollisionGroup group)
	{
		m_CollisionGroup = group;
		m_Shape->SetCollisionGroup(m_CollisionGroup);
	}

	void SphereColliderComponent::SetInteractingCollisionGroup(CollisionGroup group)
	{
		m_InteractingCollisionGroup = group;
		m_Shape->SetInteractingCollisionGroup(m_InteractingCollisionGroup);
	}

	void SphereColliderComponent::SetCollisionEnabled(bool bEnabled)
	{
		bCollisionEnabled = bEnabled;
		m_Shape->SetCollisionEnabled(bEnabled);
	}
	
	void SphereColliderComponent::OnInit()
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
	}

	void SphereColliderComponent::OnRemoved()
	{
		BaseColliderComponent::OnRemoved();
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
		if (this == &other)
			return *this;

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

	void CapsuleColliderComponent::UpdateShowCollisionState()
	{
		m_Shape->SetShowCollision(bShowCollision || Parent.GetScene()->IsForcingShowCollision());
	}

	void CapsuleColliderComponent::SetCollisionGroup(CollisionGroup group)
	{
		m_CollisionGroup = group;
		m_Shape->SetCollisionGroup(m_CollisionGroup);
	}

	void CapsuleColliderComponent::SetInteractingCollisionGroup(CollisionGroup group)
	{
		m_InteractingCollisionGroup = group;
		m_Shape->SetInteractingCollisionGroup(m_InteractingCollisionGroup);
	}

	void CapsuleColliderComponent::SetCollisionEnabled(bool bEnabled)
	{
		bCollisionEnabled = bEnabled;
		m_Shape->SetCollisionEnabled(bEnabled);
	}
	
	void CapsuleColliderComponent::SetHeightAndRadius(float height, float radius)
	{
		m_Height = glm::max(height, 0.f);
		m_Radius = glm::max(radius, 0.f);

		const auto& scale = WorldTransform.Scale3D;
		radius = glm::max(scale.x, scale.z) * m_Radius;
		height = scale.y * m_Height;

		m_Shape->SetHeightAndRadius(height, radius);
		if (IsObstacle())
			CreateObstacle();
	}
	
	void CapsuleColliderComponent::OnInit()
	{
		auto actor = Parent.GetPhysicsActor();
		if (actor)
			m_Shape = actor->AddCollider(*this);
		else
		{
			actor = Parent.GetScene()->GetPhysicsScene()->CreatePhysicsActor(Parent);
			m_Shape = actor->AddCollider(*this);
		}
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

	void CapsuleColliderComponent::OnRemoved()
	{
		BaseColliderComponent::OnRemoved();
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

	MeshColliderComponent::~MeshColliderComponent()
	{
		if (m_CollisionMeshAsset)
		{
			m_CollisionMeshAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
		}
	}

	MeshColliderComponent& MeshColliderComponent::operator=(const MeshColliderComponent& other)
	{
		if (this == &other)
			return *this;

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

	void MeshColliderComponent::UpdateShowCollisionState()
	{
		this->bShowCollision = bShowCollision;
		if (m_Shapes[0]) // No need to enable it for the backside
			m_Shapes[0]->SetShowCollision(bShowCollision || Parent.GetScene()->IsForcingShowCollision());
	}

	void MeshColliderComponent::SetCollisionGroup(CollisionGroup group)
	{
		m_CollisionGroup = group;
		for (auto& shape : m_Shapes)
		{
			if (shape)
				shape->SetCollisionGroup(m_CollisionGroup);
		}
	}

	void MeshColliderComponent::SetInteractingCollisionGroup(CollisionGroup group)
	{
		m_InteractingCollisionGroup = group;
		for (auto& shape : m_Shapes)
		{
			if (shape)
				shape->SetInteractingCollisionGroup(m_InteractingCollisionGroup);
		}
	}

	void MeshColliderComponent::SetCollisionEnabled(bool bEnabled)
	{
		bCollisionEnabled = bEnabled;
		for (auto& shape : m_Shapes)
		{
			if (shape)
				shape->SetCollisionEnabled(bEnabled);
		}
	}
	
	void MeshColliderComponent::SetCollisionMeshAsset(const Ref<AssetBaseMesh>& meshAsset)
	{
		if (meshAsset != m_CollisionMeshAsset)
		{
			if (m_CollisionMeshAsset)
			{
				m_CollisionMeshAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
			}
			m_CollisionMeshAsset = meshAsset;

			if (m_CollisionMeshAsset)
			{
				m_CollisionMeshAsset->AddOnAssetModifiedCallback(m_CallbackID, [entity = Parent]() mutable
				{
					if (entity.HasComponent<MeshColliderComponent>())
					{
						auto& comp = entity.GetComponent<MeshColliderComponent>();
						comp.SetCollisionMeshAsset(comp.GetCollisionMeshAsset());
					}
				});
			}
		}

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

		SetShowCollision(bShowCollision);
		SetIsTrigger(bTrigger);
	}
	
	void MeshColliderComponent::OnInit()
	{
		if (!m_CollisionMeshAsset && Parent)
		{
			if (Parent.HasComponent<StaticMeshComponent>())
			{
				auto& comp = Parent.GetComponent<StaticMeshComponent>();
				m_CollisionMeshAsset = comp.GetMeshAsset();
				SetRelativeTransform(comp.GetRelativeTransform());
			}
			else if (Parent.HasComponent<SkeletalMeshComponent>())
			{
				auto& comp = Parent.GetComponent<SkeletalMeshComponent>();
				m_CollisionMeshAsset = comp.GetMeshAsset();
				SetRelativeTransform(comp.GetRelativeTransform());
			}
		}
		
		SetCollisionMeshAsset(m_CollisionMeshAsset);
	}

	void MeshColliderComponent::OnRemoved()
	{
		BaseColliderComponent::OnRemoved();
		const auto& actor = Parent.GetPhysicsActor();
		if (actor)
		{
			for (auto& shape : m_Shapes)
			{
				if (shape)
				{
					actor->RemoveCollider(shape);
					shape.reset();
				}
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

		SetMeshAsset(other.m_MeshAsset);
		m_MaterialAssets = other.m_MaterialAssets;
		m_AnimAsset = other.m_AnimAsset;
		m_RootMotionLockFlags = other.m_RootMotionLockFlags;
		if (other.m_Graph)
		{
			VariablesMap copiedVars;
			for (const auto& [name, var] : other.m_Graph->GetVariables())
				copiedVars[name] = CopyVarByType(var);

			m_Graph = AnimationGraph::Create(other.m_Graph); // Copy
			m_Graph->SetVariablesToUse(copiedVars); // Forcing all graphs/subgraphs to use these variables
		}
		m_bCastsShadows = other.m_bCastsShadows;
		m_bReceivesDecals = other.m_bReceivesDecals;
		m_bVisible = other.m_bVisible;
		// Reset to 0. Otherwise if a root motion animation is playing in the editor and we press play,
		// the simulation will start from the wrong location
		CurrentClipPlayTime = 0.f;
		PrevClipPlayTime = 0.f;
		ClipPlaybackSpeed = other.ClipPlaybackSpeed;
		PrevClipPlaybackSpeed = other.PrevClipPlaybackSpeed;
		bClipLooping = other.bClipLooping;
		AnimType = other.AnimType;
		LastPose = other.LastPose;
		SetAnimationGraphAsset(other.m_AnimGraphAsset);
		SetShowRagdollCollision(other.m_bRagdollCollisionVisible);

		if (m_MeshAsset)
		{
			if (other.m_bRagdollEnabled)
			{
				SetRagdollEnabled(true);
				if (other.IsRagdollCollisionShown())
					SetShowRagdollCollision(true);
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
						const bool bCollisionEnabled = component.IsRagdollCollisionShown();
						// Recreate ragdoll
						component.SetRagdollEnabled(false);
						component.SetRagdollEnabled(true);
						if (bCollisionEnabled)
							component.SetShowRagdollCollision(true);
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
	
	void SkeletalMeshComponent::SetAnimationGraphAsset(const Ref<AssetAnimationGraph>& anim, bool bMergeVars)
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
			bMergeVars &= bSameGraph && m_Graph;

			VariablesMap oldVars;
			if (bMergeVars)
				oldVars = m_Graph->GetVariables();

			{
				VariablesMap copiedVars;
				for (const auto& [name, var] : m_AnimGraphAsset->GetGraph()->GetVariables())
					copiedVars[name] = CopyVarByType(var);

				m_Graph = AnimationGraph::Create(m_AnimGraphAsset->GetGraph()); // Copy
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

	bool SkeletalMeshComponent::HasBone(const std::string_view boneName) const
	{
		const auto& asset = GetMeshAsset();
		if (!asset)
			return false;

		return Utils::HasBone(asset->GetMesh()->GetSkeletalMeshInfo().RootBone, boneName);
	}

	Transform SkeletalMeshComponent::GetBoneWorldTransform(const std::string_view boneName) const
	{
		const auto& asset = GetMeshAsset();
		if (!asset)
			return {};

		const glm::mat4 worldTr = Math::ToTransformMatrix(GetWorldTransform());
		Transform result;
		if (IsRagdollEnabled())
		{
			Utils::GetBoneWorldTransform_Ragdoll(LastPose, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, worldTr, worldTr, boneName, &result);
		}
		else
		{
			Utils::GetBoneWorldTransform(LastPose, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, worldTr, boneName, &result);
		}
		return result;
	}

	glm::vec3 SkeletalMeshComponent::GetBoneWorldLocation(const std::string_view boneName) const
	{
		return GetBoneWorldTransform(boneName).Location;
	}

	Rotator SkeletalMeshComponent::GetBoneWorldRotation(const std::string_view boneName) const
	{
		return GetBoneWorldTransform(boneName).Rotation;
	}

	glm::vec3 SkeletalMeshComponent::GetBoneWorldScale(const std::string_view boneName) const
	{
		return GetBoneWorldTransform(boneName).Scale3D;
	}

	void SkeletalMeshComponent::SetRagdollEnabled(bool bEnabled)
	{
		if (bEnabled == m_bRagdollEnabled)
			return;

		if (bEnabled)
		{
			m_PreRagdollLastPose = LastPose;
			m_RagdollActor = Parent.GetScene()->GetPhysicsScene()->CreateRagdoll(*this);
			m_bRagdollEnabled = m_RagdollActor.operator bool();
			m_RagdollActor->SetShowCollision(m_bRagdollCollisionVisible);
		}
		else
		{
			Parent.GetScene()->GetPhysicsScene()->ReleaseRagdoll(*this);
			m_RagdollActor.reset();
			m_bRagdollEnabled = bEnabled;
			LastPose = m_PreRagdollLastPose;
		}
	}

	bool SkeletalMeshComponent::IsRagdollCollisionShown() const
	{
		return m_bRagdollCollisionVisible;
	}

	void SkeletalMeshComponent::SetShowRagdollCollision(bool bShow)
	{
		m_bRagdollCollisionVisible = bShow;
		if (m_RagdollActor)
			m_RagdollActor->SetShowCollision(bShow);
	}

	Transform SkeletalMeshComponent::GetRagdollBoneWorldTransform(const std::string& name) const
	{
		if (m_RagdollActor)
			return m_RagdollActor->GetBoneWorldTransform(name);

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::GetRagdollBoneWorldTransform. Ragdoll is null");
		return {};
	}

	Transform SkeletalMeshComponent::GetRagdollRootBoneWorldTransform() const
	{
		if (m_RagdollActor)
			return m_RagdollActor->GetRootBoneWorldTransform();

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::GetRagdollRootBoneWorldTransform. Ragdoll is null");
		return {};
	}

	void SkeletalMeshComponent::SetRagdollLinearVelocity(const glm::vec3& velocity, bool bApplyToRootOnly)
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->SetLinearVelocity(velocity, bApplyToRootOnly);
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetRagdollLinearVelocity. Ragdoll is null");
	}

	void SkeletalMeshComponent::SetRagdollAngularVelocity(const glm::vec3& velocity, bool bApplyToRootOnly)
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->SetAngularVelocity(velocity, bApplyToRootOnly);
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetRagdollAngularVelocity. Ragdoll is null");
	}

	void SkeletalMeshComponent::SetRagdollBoneLinearVelocity(const std::string& boneName, const glm::vec3& velocity)
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->SetBoneLinearVelocity(boneName, velocity);
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetRagdollBoneLinearVelocity. Ragdoll is null");
	}

	void SkeletalMeshComponent::SetRagdollBoneAngularVelocity(const std::string& boneName, const glm::vec3& velocity)
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->SetBoneAngularVelocity(boneName, velocity);
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::SetRagdollBoneAngularVelocity. Ragdoll is null");
	}

	glm::vec3 SkeletalMeshComponent::GetRagdollBoneLinearVelocity(const std::string& boneName) const
	{
		if (m_RagdollActor)
			return m_RagdollActor->GetBoneLinearVelocity(boneName);

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::GetRagdollBoneLinearVelocity. Ragdoll is null");
		return glm::vec3(0);
	}

	glm::vec3 SkeletalMeshComponent::GetRagdollBoneAngularVelocity(const std::string& boneName) const
	{
		if (m_RagdollActor)
			return m_RagdollActor->GetBoneAngularVelocity(boneName);

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::GetRagdollBoneAngularVelocity. Ragdoll is null");
		return glm::vec3(0);
	}

	void SkeletalMeshComponent::PutRagdollToSleep()
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->PutToSleep();
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::PutRagdollToSleep. Ragdoll is null");
	}

	void SkeletalMeshComponent::WakeUpRagdoll()
	{
		if (m_RagdollActor)
		{
			m_RagdollActor->WakeUp();
			return;
		}

		EG_CORE_ERROR("Failed to call SkeletalMeshComponent::WakeUpRagdoll. Ragdoll is null");
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
		PerEmitterAnimData = other.PerEmitterAnimData;
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
			UpdatePerEmitterAnimData();
			Parent.GetScene()->AddParticleSystem(*this);
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
			Parent.GetScene()->RemoveParticleSystem(*this);
			bSpawned = false;
			m_Asset->RemoveOnAssetModifiedCallback(m_SystemID);
		}
		PerEmitterAnimData.clear();
	}
	
	void ParticleSystemComponent::Update()
	{
		UpdatePerEmitterAnimData();
		if (bSpawned)
			Parent.GetScene()->UpdateParticleSystem(*this);
	}

	void ParticleSystemComponent::UpdatePerEmitterAnimData()
	{
		const auto& emitters = m_Asset->GetEmitters();
		const size_t emittersCount = emitters.size();
		PerEmitterAnimData.resize(emittersCount);
		for (size_t i = 0; i < emittersCount; ++i)
		{
			const auto& emitter = emitters[i];
			auto& data = PerEmitterAnimData[i];

			if (!emitter.IsSkeletalMeshUsed())
			{
				data.CurrentClipPlayTime = 0.f;
				data.PrevClipPlayTime = 0.f;
			}
			else
			{
				const auto& animAsset = emitter.MeshAnimationAsset;
				const SkeletalMeshAnimation* animation = animAsset ? animAsset->GetAnimation().get() : nullptr;
				if (animation)
				{
					// Make sure we don't go out of bounds if animation was changed
					data.CurrentClipPlayTime = AnimationSystem::WrapAnimationTime(animation->Duration, data.CurrentClipPlayTime, emitter.bClipLooping);
					data.PrevClipPlayTime = AnimationSystem::WrapAnimationTime(animation->Duration, data.PrevClipPlayTime, emitter.bClipLooping);
				}
				else
				{
					data.CurrentClipPlayTime = 0.f;
					data.PrevClipPlayTime = 0.f;
				}
			}
		}
	}
	
	NavigationMeshComponent& NavigationMeshComponent::operator=(const NavigationMeshComponent& other)
	{
		if (this == &other)
			return *this;

		SceneComponent::operator=(other);
		bAutoRebuild = other.bAutoRebuild;
		m_Settings = other.m_Settings;
		m_CrowdSettings = other.m_CrowdSettings;
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
		settings.AABB.Transform(worldTr);

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
				EG_CORE_ASSERT(overlap.HitEntity.HasComponent<BoxColliderComponent>());
				collider = &overlap.HitEntity.GetComponent<BoxColliderComponent>();
				break;
			case ColliderType::Sphere:
				EG_CORE_ASSERT(overlap.HitEntity.HasComponent<SphereColliderComponent>());
				collider = &overlap.HitEntity.GetComponent<SphereColliderComponent>();
				break;
			case ColliderType::Capsule:
				EG_CORE_ASSERT(overlap.HitEntity.HasComponent<CapsuleColliderComponent>());
				collider = &overlap.HitEntity.GetComponent<CapsuleColliderComponent>();
				break;
			case ColliderType::ConvexMesh:
			case ColliderType::TriangleMesh:
				EG_CORE_ASSERT(overlap.HitEntity.HasComponent<MeshColliderComponent>());
				collider = &overlap.HitEntity.GetComponent<MeshColliderComponent>();
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
	
	void NativeScriptComponent::OnUpdate(Entity entity, Timestep ts)
	{
		InitScriptsIfNeeded(entity);
		if (Instance)
			Instance->OnUpdate(ts);
	}
	
	void NativeScriptComponent::OnEvent(Entity entity, Event& e)
	{
		InitScriptsIfNeeded(entity);
		if (Instance)
			Instance->OnEvent(e);
	}

	void NativeScriptComponent::Destroy()
	{
		if (Instance)
		{
			Instance->OnDestroy();
			Instance.reset();
		}
		m_TypeHash = 0;
	}
}
