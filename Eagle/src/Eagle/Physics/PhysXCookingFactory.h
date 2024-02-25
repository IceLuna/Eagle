#pragma once

#include <glm/glm.hpp>
#include "PhysicsUtils.h"

namespace Eagle
{
	class AssetStaticMesh;

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

		static CookingResult CookMesh(const Ref<AssetStaticMesh>& collisionMeshAsset, bool bConvex, bool bFlipNormals, bool bInvalidateOld = false, MeshColliderData& outData = MeshColliderData());

	private:
		static CookingResult CookConvexMesh(const Ref<AssetStaticMesh>& meshAsset, MeshColliderData& outData = MeshColliderData());
		static CookingResult CookTriangleMesh(const Ref<AssetStaticMesh>& meshAsset, bool bFlip, MeshColliderData& outData = MeshColliderData());
	};
}
