#pragma once

#include "PhysicsEngine.h"
#include "Eagle/Core/Entity.h"

#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	struct BoneNode;
	struct SkeletalPose;

	class PhysicsRagdollActor
	{
	public:
		PhysicsRagdollActor(Entity entity, physx::PxScene* scene, const PhysicsSettings& settings);
		~PhysicsRagdollActor();

		float GetSimulationTimeStep() const { return m_Settings.FixedTimeStep; }
		const Entity& GetEntity() const { return m_Entity; }

		void SynchronizeTransform();
		bool DoesNeedSync() const { return m_bDirtyTransform; }
		void MarkTransformDirty() { m_bDirtyTransform = true; }

		bool IsCollisionShown() const { return m_bShowCollision; }
		void SetShowCollision(bool bShowCollision);
		Transform GetBoneWorldTransform(const std::string& boneName) const;
		const physx::PxRigidActor* GetPhysXActor() const { return m_Root.Body; }

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
		Entity m_Entity;
		physx::PxScene* m_Scene = nullptr;
		PhysicsSettings m_Settings;
		glm::mat4 m_OriginalTransformInv = glm::mat4(1.f);
		bool m_bDirtyTransform = true;
		bool m_bShowCollision = false;

		BoneData m_Root;
		std::unordered_map<std::string, physx::PxRigidDynamic*> m_BonesMap;
		PhysicsActorPayload m_Payload;
	};
}
