#pragma once

#include "Eagle/Core/Entity.h"
#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	// Base class for physics actors
	class PhysicsActorBase
	{
	public:
		PhysicsActorBase(const Entity& entity) : m_Entity(entity) {}

		// It's called when physics scene requested to sync transforms
		virtual void SceneRequestToSyncTransforms() {}

		const physx::PxRigidActor* GetPhysXActor() const { return m_RigidActor; }
		physx::PxRigidActor* GetPhysXActor() { return m_RigidActor; }

		const Entity& GetEntity() const { return m_Entity; }

	protected:
		Entity m_Entity;
		physx::PxRigidActor* m_RigidActor = nullptr;
	};
}
