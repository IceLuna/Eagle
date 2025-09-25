#pragma once

#include <glm/glm.hpp>
#include "PhysicsUtils.h"

namespace Eagle
{
	class AssetStaticMesh;

	class MeshColliderComponent;
	class StaticMesh;

	class PhysXCookingFactory
	{
	public:
		static void Init();
		static void Shutdown();

		static CookingResult CookMesh(const Ref<AssetStaticMesh>& collisionMeshAsset, bool bConvex, bool bFlipNormals, ScopedDataBuffer* outData);

	private:
		static CookingResult CookConvexMesh(const Ref<AssetStaticMesh>& meshAsset, ScopedDataBuffer* outData);
		static CookingResult CookTriangleMesh(const Ref<AssetStaticMesh>& meshAsset, bool bFlip, ScopedDataBuffer* outData);
	};
}
