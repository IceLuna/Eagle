#include "egpch.h"
#include "ContactListener.h"
#include "PhysicsActor.h"
#include "PhysicsRagdollActor.h"
#include "PhysicsScene.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	void ContactListener::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32)
	{
	}
	
	void ContactListener::onWake(physx::PxActor** actors, physx::PxU32 count)
	{
#if 0
		for (uint32_t i = 0; i < count; ++i)
		{
			physx::PxActor& physxActor = *actors[i];

			Entity entity;
			if (physxActor.userData)
			{
				const PhysicsActorBase* actor = (PhysicsActorBase*)physxActor.userData;
				entity = actor->GetEntity();
			}
			//EG_CORE_INFO("[Physics Engine] Physics Actor is waking up. Name {0}", entity.GetComponent<EntitySceneNameComponent>().Name);
		}
#endif
	}
	
	void ContactListener::onSleep(physx::PxActor** actors, physx::PxU32 count)
	{
#if 0
		for (uint32_t i = 0; i < count; ++i)
		{
			physx::PxActor& physxActor = *actors[i];

			Entity entity;
			if (physxActor.userData)
			{
				const PhysicsActorBase* actor = (PhysicsActorBase*)physxActor.userData;
				entity = actor->GetEntity();
			}
			//EG_CORE_INFO("[Physics Engine] Physics Actor is going to sleep. Name {0}", entity.GetComponent<EntitySceneNameComponent>().Name);
		}
#endif
	}
	
	void ContactListener::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
	{
		if (!Scene::GetCurrentScene()->IsPlaying())
			return;

		auto removedActorA = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0;
		auto removedActorB = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1;

		if (removedActorA || removedActorB)
			return;

		Entity entityA;
		Entity entityB;

		// Actor A
		if (void* userData = pairHeader.actors[0]->userData)
		{
			const PhysicsActorBase* actor = (PhysicsActorBase*)userData;
			entityA = actor->GetEntity();
		}
		
		// Actor B
		if (void* userData = pairHeader.actors[1]->userData)
		{
			const PhysicsActorBase* actor = (PhysicsActorBase*)userData;
			entityA = actor->GetEntity();
		}

		bool bActorAHasScript = ScriptEngine::IsEntityModuleValid(entityA);
		bool bActorBHasScript = ScriptEngine::IsEntityModuleValid(entityB);

		CollisionInfo collisionInfo{};
		if (nbPairs > 0)
		{
			using namespace physx;

			physx::PxContactPairPoint contact;
			PxU32 nbContacts = pairs[0].extractContacts(&contact, 1);
			if (nbContacts > 0)
			{
				const float simulationTimeStep = ((PhysicsScene*)(pairHeader.actors[0]->getScene()->userData))->GetSimulationTimeStep();

				collisionInfo.Position = PhysXUtils::FromPhysXVector(contact.position);
				collisionInfo.Impulse = PhysXUtils::FromPhysXVector(contact.impulse);
				collisionInfo.Force = collisionInfo.Impulse * simulationTimeStep;
				collisionInfo.Normal = PhysXUtils::FromPhysXVector(contact.normal);
			}
		}

		if (!bActorAHasScript && !bActorBHasScript)
			return;

		if ((pairs->flags & physx::PxContactPairFlag::eACTOR_PAIR_HAS_FIRST_TOUCH) == physx::PxContactPairFlag::eACTOR_PAIR_HAS_FIRST_TOUCH)
		{
			if (bActorAHasScript)
				ScriptEngine::OnCollisionBegin(entityA, entityB, collisionInfo);
			if (bActorBHasScript)
				ScriptEngine::OnCollisionBegin(entityB, entityA, collisionInfo);
		}
		else if ((pairs->flags & physx::PxContactPairFlag::eACTOR_PAIR_LOST_TOUCH) == physx::PxContactPairFlag::eACTOR_PAIR_LOST_TOUCH)
		{
			if (bActorAHasScript)
				ScriptEngine::OnCollisionEnd(entityA, entityB, collisionInfo);
			if (bActorBHasScript)
				ScriptEngine::OnCollisionEnd(entityB, entityA, collisionInfo);
		}
	}
	
	void ContactListener::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
	{
		if (!Scene::GetCurrentScene()->IsPlaying())
			return;

		for (uint32_t i = 0; i < count; ++i)
		{
			if (pairs[i].flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;
			
			Entity triggerEntity;
			Entity otherEntity;

			// Actor A
			if (void* userData = pairs[i].triggerActor->userData)
			{
				const PhysicsActorBase* actor = (PhysicsActorBase*)userData;
				triggerEntity = actor->GetEntity();
			}

			// Actor B
			if (void* userData = pairs[i].otherActor->userData)
			{
				const PhysicsActorBase* actor = (PhysicsActorBase*)userData;
				otherEntity = actor->GetEntity();
			}

			bool bTriggerHasScript = ScriptEngine::IsEntityModuleValid(triggerEntity);
			bool bOtherHasScript = ScriptEngine::IsEntityModuleValid(otherEntity);

			if (!bTriggerHasScript && !bOtherHasScript)
				continue;

			if ((pairs[i].status & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
			{
				if (bTriggerHasScript)
					ScriptEngine::OnTriggerBegin(triggerEntity, otherEntity);
				if (bOtherHasScript)
					ScriptEngine::OnTriggerBegin(otherEntity, triggerEntity);
			}
			else if ((pairs[i].status & physx::PxPairFlag::eNOTIFY_TOUCH_LOST) == physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
			{
				if (bTriggerHasScript)
					ScriptEngine::OnTriggerEnd(triggerEntity, otherEntity);
				if (bOtherHasScript)
					ScriptEngine::OnTriggerEnd(otherEntity, triggerEntity);
			}
		}
	}
	
	void ContactListener::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32)
	{
	}
}
