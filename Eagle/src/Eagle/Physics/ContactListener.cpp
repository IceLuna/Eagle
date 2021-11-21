#include "egpch.h"
#include "ContactListener.h"
#include "PhysicsActor.h"
#include "Eagle/Script/ScriptEngine.h"

namespace Eagle
{
	void ContactListener::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32)
	{
	}
	
	void ContactListener::onWake(physx::PxActor** actors, physx::PxU32 count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			physx::PxActor& physxActor = *actors[i];
			PhysicsActor* actor = (PhysicsActor*)physxActor.userData;
			const Entity& entity = actor->GetEntity();
			EG_CORE_INFO("[Physics Engine] Physics Actor is waking up. Name {0}", entity.GetComponent<EntitySceneNameComponent>().Name);
		}
	}
	
	void ContactListener::onSleep(physx::PxActor** actors, physx::PxU32 count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			physx::PxActor& physxActor = *actors[i];
			PhysicsActor* actor = (PhysicsActor*)physxActor.userData;
			const Entity& entity = actor->GetEntity();
			EG_CORE_INFO("[Physics Engine] Physics Actor is going to sleep. Name {0}", entity.GetComponent<EntitySceneNameComponent>().Name);
		}
	}
	
	void ContactListener::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
	{
		if (!Scene::GetCurrentScene()->IsPlaying())
			return;

		auto removedActorA = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0;
		auto removedActorB = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1;

		if (removedActorA || removedActorB)
			return;

		PhysicsActor* actorA = (PhysicsActor*)pairHeader.actors[0]->userData;
		PhysicsActor* actorB = (PhysicsActor*)pairHeader.actors[1]->userData;

		bool bActorAHasScript = ScriptEngine::IsEntityModuleValid(actorA->GetEntity());
		bool bActorBHasScript = ScriptEngine::IsEntityModuleValid(actorB->GetEntity());

		if (!bActorAHasScript && !bActorBHasScript)
			return;

		if (pairs->flags == physx::PxContactPairFlag::eACTOR_PAIR_HAS_FIRST_TOUCH)
		{
			if (bActorAHasScript)
				ScriptEngine::OnCollisionBegin(actorA->GetEntity(), actorB->GetEntity());
			if (bActorBHasScript)
				ScriptEngine::OnCollisionBegin(actorB->GetEntity(), actorA->GetEntity());
		}
		else if (pairs->flags == physx::PxContactPairFlag::eACTOR_PAIR_LOST_TOUCH)
		{
			if (bActorAHasScript)
				ScriptEngine::OnCollisionEnd(actorA->GetEntity(), actorB->GetEntity());
			if (bActorBHasScript)
				ScriptEngine::OnCollisionEnd(actorB->GetEntity(), actorA->GetEntity());
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
			
			PhysicsActor* triggerActor = (PhysicsActor*)pairs[i].triggerActor->userData;
			PhysicsActor* otherActor = (PhysicsActor*)pairs[i].otherActor->userData;

			if (!triggerActor || !otherActor)
				continue;

			bool bTriggerHasScript = ScriptEngine::IsEntityModuleValid(triggerActor->GetEntity());
			bool bOtherHasScript = ScriptEngine::IsEntityModuleValid(otherActor->GetEntity());

			if (!bTriggerHasScript && !bOtherHasScript)
				continue;

			if (pairs[i].status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
			{
				if (bTriggerHasScript)
					ScriptEngine::OnTriggerBegin(triggerActor->GetEntity(), otherActor->GetEntity());
				if (bOtherHasScript)
					ScriptEngine::OnTriggerBegin(otherActor->GetEntity(), triggerActor->GetEntity());
			}
			else if (pairs[i].status == physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
			{
				if (bTriggerHasScript)
					ScriptEngine::OnTriggerEnd(triggerActor->GetEntity(), otherActor->GetEntity());
				if (bOtherHasScript)
					ScriptEngine::OnTriggerEnd(otherActor->GetEntity(), triggerActor->GetEntity());
			}
		}
	}
	
	void ContactListener::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32)
	{
	}
}
