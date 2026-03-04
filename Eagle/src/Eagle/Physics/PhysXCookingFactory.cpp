#include "egpch.h"
#include "PhysXCookingFactory.h"
#include "PhysXInternal.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	struct PhysXCookingData
	{
		physx::PxCooking* CookingSDK;
		physx::PxCookingParams CookingParams;

		PhysXCookingData(const physx::PxTolerancesScale& scale)
		: CookingSDK(nullptr), CookingParams(scale) {}
	};

	static PhysXCookingData* s_CookingData = nullptr;

	template <typename AssetMeshType>
	CookingResult CookConvexMesh(const Ref<AssetMeshType>& meshAsset, ScopedDataBuffer* outData)
	{
		constexpr bool bStatic = std::is_same_v<AssetMeshType, AssetStaticMesh>;
		constexpr bool bSkeletal = std::is_same_v<AssetMeshType, AssetSkeletalMesh>;
		static_assert(bStatic || bSkeletal, "Invalid type. Should be `StaticMesh` or `SkeletalMesh`");

		const auto& mesh = meshAsset->GetMesh();
		const auto& vertices = mesh->GetVertices();

		size_t indicesCount = 0;
		const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
		for (uint32_t i = 0; i < materialsCount; ++i)
			indicesCount += mesh->GetIndicesCount(i);

		std::vector<Index> indices;
		indices.reserve(indicesCount);

		for (uint32_t i = 0; i < materialsCount; ++i)
		{
			const auto& inserting = mesh->GetIndices(i);
			indices.insert(indices.end(), inserting.begin(), inserting.end());
		}

		physx::PxConvexMeshDesc convexDesc;
		convexDesc.points.count = (uint32_t)vertices.size();
		convexDesc.points.stride = bStatic ? sizeof(Vertex) : sizeof(SkeletalVertex);
		convexDesc.points.data = &vertices[0];
		convexDesc.indices.count = (uint32_t)indices.size() / 3;
		convexDesc.indices.stride = sizeof(Index) * 3;
		convexDesc.indices.data = &indices[0];
		convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eSHIFT_VERTICES | physx::PxConvexFlag::eQUANTIZE_INPUT;

		physx::PxDefaultMemoryOutputStream buf;
		physx::PxConvexMeshCookingResult::Enum result;
		if (!s_CookingData->CookingSDK->cookConvexMesh(convexDesc, buf, &result))
		{
			EG_CORE_ERROR("[Physics Engine] Failed to cook convex mesh '{0}'. Reason: {1}", meshAsset->GetPath().u8string(), Utils::GetEnumName(result));
			return PhysXUtils::FromPhysXCookingResult(result);
		}

		outData->Allocate(buf.getSize());
		memcpy(outData->Data(), buf.getData(), outData->Size());

		return CookingResult::Success;
	}

	template <typename AssetMeshType>
	CookingResult CookTriangleMesh(const Ref<AssetMeshType>& meshAsset, bool bFlipNormals, ScopedDataBuffer* outData)
	{
		constexpr bool bStatic = std::is_same_v<AssetMeshType, AssetStaticMesh>;
		constexpr bool bSkeletal = std::is_same_v<AssetMeshType, AssetSkeletalMesh>;
		static_assert(bStatic || bSkeletal, "Invalid type. Should be `StaticMesh` or `SkeletalMesh`");

		const auto& mesh = meshAsset->GetMesh();
		const auto& vertices = mesh->GetVertices();

		size_t indicesCount = 0;
		const uint32_t materialsCount = mesh->GetMaterialSlotsCount();
		for (uint32_t i = 0; i < materialsCount; ++i)
			indicesCount += mesh->GetIndicesCount(i);

		std::vector<Index> indices;
		indices.reserve(indicesCount);

		for (uint32_t i = 0; i < materialsCount; ++i)
		{
			const auto& inserting = mesh->GetIndices(i);
			indices.insert(indices.end(), inserting.begin(), inserting.end());
		}

		physx::PxTriangleMeshDesc triangleDesc;
		triangleDesc.points.count = (uint32_t)vertices.size();
		triangleDesc.points.stride = bStatic ? sizeof(Vertex) : sizeof(SkeletalVertex);
		triangleDesc.points.data = &vertices[0];
		triangleDesc.triangles.count = (uint32_t)indices.size() / 3;
		triangleDesc.triangles.stride = sizeof(Index) * 3;
		triangleDesc.triangles.data = &indices[0];
		if (bFlipNormals)
			triangleDesc.flags |= physx::PxMeshFlag::eFLIPNORMALS;

#if 0
		bool bValid = s_CookingData->CookingSDK->validateTriangleMesh(triangleDesc);
		if (!bValid)
		{
			EG_CORE_ERROR("[Physics Engine] Failed to validate triangle mesh '{0}'", meshAsset->GetPath().u8string());
			return CookingResult::Failure;
		}
#endif

		physx::PxDefaultMemoryOutputStream buf;
		physx::PxTriangleMeshCookingResult::Enum result;
		if (!s_CookingData->CookingSDK->cookTriangleMesh(triangleDesc, buf, &result))
		{
			EG_CORE_ERROR("[Physics Engine] Failed to cook triangle mesh '{0}'. Reason: {1}", meshAsset->GetPath().u8string(), Utils::GetEnumName(result));
			return PhysXUtils::FromPhysXCookingResult(result);
		}

		outData->Allocate(buf.getSize());
		memcpy(outData->Data(), buf.getData(), outData->Size());

		return CookingResult::Success;
	}

	void PhysXCookingFactory::Init()
	{
		EG_CORE_ASSERT(!s_CookingData, "[Physics Engine] Trying to init Cooking Factory twice!");

		static bool bSupportsSSE2 = Utils::IsSSE2Supported();
		static auto midphaseDesc = bSupportsSSE2 ? physx::PxMeshMidPhase::eBVH34 : physx::PxMeshMidPhase::eBVH33;

		s_CookingData = new PhysXCookingData(PhysXInternal::GetPhysics().getTolerancesScale());
		s_CookingData->CookingParams.meshWeldTolerance = 0.1f;
		s_CookingData->CookingParams.meshPreprocessParams = physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
		s_CookingData->CookingParams.midphaseDesc = midphaseDesc;

		s_CookingData->CookingSDK = PxCreateCooking(PX_PHYSICS_VERSION, PhysXInternal::GetFoundation(), s_CookingData->CookingParams);
		EG_CORE_ASSERT(s_CookingData->CookingSDK, "[Physics Engine] Failed to create Cooking");
	}
	
	void PhysXCookingFactory::Shutdown()
	{
		s_CookingData->CookingSDK->release();
		s_CookingData->CookingSDK = nullptr;

		delete s_CookingData;
		s_CookingData = nullptr;
	}

	CookingResult PhysXCookingFactory::CookMesh(const Ref<AssetBaseMesh>& collisionMesh, bool bConvex, bool bFlip, ScopedDataBuffer* outData)
	{
		if (!collisionMesh)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking: Invalid mesh!");
			return CookingResult::Failure;
		}

		std::string filename = collisionMesh->GetPath().stem().u8string();
		if (bConvex)
			filename += "_convex.pxm";
		else
		{
			if (bFlip)
				filename += "_flipped";
			filename += "_tri.pmx";
		}

		const Path filepath = Project::GetCachePath() / "PhysX" / filename;

		CookingResult result = CookingResult::Failure;
		if (!std::filesystem::exists(filepath))
		{
			if (collisionMesh->GetAssetType() == AssetType::StaticMesh)
			{
				Ref<AssetStaticMesh> staticMesh = Cast<AssetStaticMesh>(collisionMesh);
				result = bConvex ? CookConvexMesh(staticMesh, outData) : CookTriangleMesh(staticMesh, bFlip, outData);
			}
			else if(collisionMesh->GetAssetType() == AssetType::SkeletalMesh)
			{
				Ref<AssetSkeletalMesh> skeletalMesh = Cast<AssetSkeletalMesh>(collisionMesh);
				result = bConvex ? CookConvexMesh(skeletalMesh, outData) : CookTriangleMesh(skeletalMesh, bFlip, outData);
			}
			else
			{
				EG_CORE_ASSERT(false, "Unknown mesh type");
				result = CookingResult::Failure;
			}

			if (result == CookingResult::Success)
			{
				bool bSuccessWrite = FileSystem::Write(filepath, *outData);

				if (!bSuccessWrite)
					EG_CORE_ERROR("[Physics Engine] Failed to write collider to '{0}'", filepath.u8string());
			}
		}
		else
		{
			*outData = FileSystem::Read(filepath);
			if (outData->Size() > 0)
			{
				result = CookingResult::Success;
			}
		}

		return result;
	}
}
