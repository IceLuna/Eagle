#pragma once

#include "Eagle/Core/GUID.h"
#include "PhysicsActor.h"
#include <PhysX/PxPhysicsAPI.h>
#include <glm/glm.hpp>

namespace Eagle
{
	struct RaycastHit
	{
		GUID HitEntity;
		glm::vec3 Position;
		glm::vec3 Normal;
		float Distance;
	};

	class PhysicsScene
	{
	public:
		
	private:
		physx::PxScene* m_Scene;
		std::vector<Ref<PhysicsActor>> m_Actors;

	};
}
