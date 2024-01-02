#pragma once

#include <glm/glm.hpp>
#include "PhysicsUtils.h"

namespace Eagle
{
	class AssetMesh;

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

		static CookingResult CookMesh(const Ref<AssetMesh>& collisionMeshAsset, bool bConvex, bool bFlipNormals, bool bInvalidateOld = false, MeshColliderData& outData = MeshColliderData());

	private:
		static CookingResult CookConvexMesh(const Ref<AssetMesh>& meshAsset, MeshColliderData& outData = MeshColliderData());
		static CookingResult CookTriangleMesh(const Ref<AssetMesh>& meshAsset, bool bFlip, MeshColliderData& outData = MeshColliderData());
	};
}
