#pragma once

#include "PhysicsEngine.h"
#include "PhysicsActorBase.h"
#include "Eagle/Core/Entity.h"

#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	struct BoneNode;
	struct SkeletalPose;

	class PhysicsRagdollActor : public PhysicsActorBase
	{
	public:
		PhysicsRagdollActor(Entity entity, physx::PxScene* scene);
		~PhysicsRagdollActor();

		void SceneRequestToSyncTransforms() override
		{
			// `SynchronizeTransform()` call is postponed. It'll be called by `AnimationSystem` in a multithreaded manner
			MarkTransformDirty();
		}

		void SynchronizeTransform();
		bool DoesNeedSync() const { return m_bDirtyTransform; }

		// The final bone transforms last computed by `AnimationSystem::UpdateRagdolls`.
		// The animation update copies them, so that the renderer gets valid transforms even if a script removes the ragdoll before rendering
		std::vector<glm::mat4>& GetBoneTransforms() { return m_BoneTransforms; }
		const std::vector<glm::mat4>& GetBoneTransforms() const { return m_BoneTransforms; }

		// Moves all bodies the way the entity moved from `from` to `to`, keeping their poses relative to it. The scale is ignored
		void Teleport(const Transform& from, const Transform& to, bool bResetVelocity);
		void MarkTransformDirty() { m_bDirtyTransform = true; }

		bool IsCollisionShown() const { return m_bShowCollision; }
		void SetShowCollision(bool bShowCollision);
		Transform GetBoneWorldTransform(const std::string& boneName) const;
		Transform GetRootBoneWorldTransform() const;

		// @bApplyToRootOnly. If false, then all updates all ragdoll bones
		void SetLinearVelocity(const glm::vec3& velocity, bool bApplyToRootOnly);
		void SetAngularVelocity(const glm::vec3& velocity, bool bApplyToRootOnly);
		void AddForce(const glm::vec3& force, ForceMode forceMode, bool bApplyToRootOnly);
		void AddForceAtLocation(const glm::vec3& location, const glm::vec3& force, ForceMode forceMode, bool bApplyToRootOnly);
		void AddTorque(const glm::vec3& torque, ForceMode forceMode, bool bApplyToRootOnly);

		// Return velocity of the root bone
		glm::vec3 GetLinearVelocity() const;
		glm::vec3 GetAngularVelocity() const;

		void SetBoneLinearVelocity(const std::string& boneName, const glm::vec3& velocity);
		glm::vec3 GetBoneLinearVelocity(const std::string& boneName) const;

		void SetBoneAngularVelocity(const std::string& boneName, const glm::vec3& velocity);
		glm::vec3 GetBoneAngularVelocity(const std::string& boneName) const;

		void AddBoneForce(const std::string& boneName, const glm::vec3& force, ForceMode forceMode);
		void AddBoneForceAtLocation(const std::string& boneName, const glm::vec3& location, const glm::vec3& force, ForceMode forceMode);
		void AddBoneTorque(const std::string& boneName, const glm::vec3& torque, ForceMode forceMode);

		void PutToSleep();
		void WakeUp();

	public:
		struct BoneData
		{
			std::string Name;
			std::vector<BoneData> Children;
			glm::mat4 BoneWorldTr = glm::mat4(1.f);
			glm::mat4 OriginalBodyTrInv = glm::mat4(1.f);
			bool bValidBone = true;

			physx::PxShape* Shape = nullptr;
			physx::PxRigidDynamic* Body = nullptr;
			physx::PxD6Joint* Joint = nullptr;
		};

	private:
		physx::PxScene* m_Scene = nullptr;
		std::vector<glm::mat4> m_BoneTransforms;
		bool m_bDirtyTransform = true;
		bool m_bShowCollision = false;

		BoneData m_Root;
		ankerl::unordered_dense::map<std::string, physx::PxRigidDynamic*> m_BonesMap;
	};
}
