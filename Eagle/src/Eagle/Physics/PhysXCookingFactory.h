#pragma once

#include <glm/glm.hpp>
#include "PhysicsUtils.h"

namespace Eagle
{
	struct MeshColliderData
	{
		uint8_t* Data = nullptr;
		uint32_t Size = 0;
	};

	class MeshColliderComponent;
	class StaticMesh;

	class PhysXCookingFactory
	{
	public:
		static void Init();
		static void Shutdown();

		static CookingResult CookMesh(MeshColliderComponent& component, bool bInvalidateOld = false, MeshColliderData& outData = MeshColliderData());

	private:
		static CookingResult CookConvexMesh(const Ref<StaticMesh>& mesh, MeshColliderData& outData = MeshColliderData());
		static CookingResult CookTriangleMesh(const Ref<StaticMesh>& mesh, bool bFlipNormals, MeshColliderData& outData = MeshColliderData());
	};
}
