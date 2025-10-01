#pragma once

#include <glm/glm.hpp>
#include "PhysicsUtils.h"

namespace Eagle
{
	class AssetBaseMesh;

	class PhysXCookingFactory
	{
	public:
		static void Init();
		static void Shutdown();

		static CookingResult CookMesh(const Ref<AssetBaseMesh>& collisionMeshAsset, bool bConvex, bool bFlipNormals, ScopedDataBuffer* outData);
	};
}
