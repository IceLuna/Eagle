#pragma once

#include "PhysicsEngine.h"
#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	class CharacterControllerComponent;
	class PhysicsScene;
	class AssetPhysicsMaterial;

	class PhysicsCharacterController
	{
	public:
		PhysicsCharacterController(const CharacterControllerComponent& component);
		PhysicsCharacterController(const PhysicsCharacterController&) = delete;
		PhysicsCharacterController(PhysicsCharacterController&&) = delete;
		~PhysicsCharacterController();

		PhysicsCharacterController& operator= (const PhysicsCharacterController&) = delete;
		PhysicsCharacterController& operator= (PhysicsCharacterController&&) = delete;

		// Moves the character. To retrieve new position, use `GetWorldLocation()` or `GetFootWorldLocation()`
		// @disp is the displacement vector for current frame. It is typically a combination of vertical motion due to gravity and lateral motion when your character is moving.
		//		 Note that users are responsible for applying gravity to characters here.
		// @minDist is a minimal length used to stop the recursive displacement algorithm early when remaining distance to travel goes below this limit.
		// @elapsedTime is the amount of time that passed since the last call to the move function.
		// @return Returns collision flags
		CharacterControllerCollisionFlags Move(const glm::vec3& disp, float minDist, float elapsedTime);

		void SetSlopeLimit(float degrees);
		float GetSlopeLimit() const { return m_SlopeLimit; }

		void SetContactOffset(float contactOffset);
		float GetContactOffset() const { return m_ContactOffset; }

		void SetStepOffset(float stepOffset);
		float GetStepOffset() const { return m_StepOffset; }

		void SetPhysicsMaterialAsset(const Ref<AssetPhysicsMaterial>& material);
		const Ref<AssetPhysicsMaterial>& GetPhysicsMaterialAsset() const { return m_PhysicsMaterial; }

		void SetShapeType(CharacterControllerShape shape);
		CharacterControllerShape GetShapeType() const { return m_Shape; }

		void SetCapsuleClimbingMode(CapsuleClimbingMode mode);
		CapsuleClimbingMode GetCapsuleClimbingMode() const { return m_ClimbingMode; }

		void SetCapsuleRadius(float radius);
		float GetCapsuleRadius() const { return m_Radius; }

		void SetCapsuleHeight(float height);
		float GetCapsuleHeight() const { return m_Height; }

		void SetBoxHalfExtent(const glm::vec3& halfExtent);
		const glm::vec3& GetBoxHalfExtent() const { return m_HalfExtent; }

		void SetWorldLocation(const glm::vec3& location);
		glm::vec3 GetWorldLocation() const;

		// "foot" position of the controller, i.e.the position of the bottom of the CCT's shape.
	    // The foot position takes the contact offset into account
		void SetFootWorldLocation(const glm::vec3& location);
		glm::vec3 GetFootWorldLocation() const;

		// If set to true, controller vs controller collisions will be resolved using collision groups.
		// If set to false, controllers won't collide with each other
		void SetDoesCollideWithOtherControllers(bool bCollides) { bCollidesWithOtherControllers = bCollides; }
		bool DoesCollideWithOtherControllers() const { return bCollidesWithOtherControllers; }

		// Collision groups it belongs to. It can belong to different groups (use XOR to combine groups)
		void SetCollisionGroup(CollisionGroup groups) { m_CollisionGroup = groups; }
		CollisionGroup GetCollisionGroup() const { return m_CollisionGroup; }

		// Collision groups it can interact with
		void SetInteractingCollisionGroup(CollisionGroup groups) { m_InteractingCollisionGroup = groups; }
		CollisionGroup GetInteractingCollisionGroup() const { return m_InteractingCollisionGroup; }

		// `upDir` should be normalized
		void SetUpDirection(const glm::vec3& upDir);
		const glm::vec3& GetUpDirection() const { return m_Up; }

	private:
		void Recreate(physx::PxExtendedVec3 location);
		void IterateShapes(const std::function<void(physx::PxShape*)>& func);

	private:
		Weak<PhysicsScene> m_Scene;
		physx::PxController* m_Controller = nullptr;

		CollisionGroup m_CollisionGroup = s_DefaultCollisionGroup;
		CollisionGroup m_InteractingCollisionGroup = s_DefaultInteractingCollisionGroup;

		// Can be used to rotate the controller
		glm::vec3 m_Up = glm::vec3(0, 1, 0);

		// If set to true, controller vs controller collisions will be resolved using collision groups.
		// If set to false, controllers won't collide with each other
		bool bCollidesWithOtherControllers = true;

		// The maximum slope which the character can walk up.
		// In general it is desirable to limit where the character can walk, in particular it is unrealistic
		// for the character to be able to climb arbitary slopes.
		// A value of 0 disables this feature.
		// It is currently enabled for static actors only (not for dynamic/kinematic actors), and not supported for spheres or capsules.
		float m_SlopeLimit = 45.0f; // Degrees

		// The contact offset used by the controller.
		// Specifies a skin around the object within which contacts will be generated.
		// Use it to avoid numerical precision issues.
		// This is dependant on the scale of the users world, but should be a small, positive
		// non zero value.
		// Basically it's: "How close can I get to geometry before physics engine starts pushing me away?"
		float m_ContactOffset = 0.02f;

		// Defines the maximum height of an obstacle which the character can climb.
		// A small value will mean that the character gets stuck and cannot walk up stairs etc,
		// a value which is too large will mean that the character can climb over unrealistically
		// high obstacles.
		float m_StepOffset = 0.35f;

		Ref<AssetPhysicsMaterial> m_PhysicsMaterial;

		CharacterControllerShape m_Shape = CharacterControllerShape::Capsule;
		CapsuleClimbingMode m_ClimbingMode = CapsuleClimbingMode::Easy;

		// Capsule
		float m_Radius = 0.35f;
		float m_Height = 1.0f;

		// Box
		glm::vec3 m_HalfExtent = glm::vec3(0.5f);
	};
}
