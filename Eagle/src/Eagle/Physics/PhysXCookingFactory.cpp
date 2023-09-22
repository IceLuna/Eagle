#include "egpch.h"
#include "PhysXCookingFactory.h"
#include "PhysXInternal.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Utils/PlatformUtils.h"

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

	CookingResult PhysXCookingFactory::CookMesh(MeshColliderComponent& component, bool bInvalidateOld, MeshColliderData& outData)
	{
		const auto& collisionMesh = component.GetCollisionMesh();

		if (!collisionMesh)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking: Invalid mesh!");
			return CookingResult::Failure;
		}

		const bool bConvex = component.IsConvex();
		const bool bFlip = component.IsFlipped();

		std::string filename = collisionMesh->GetPath().stem().u8string() + "_" + collisionMesh->GetName();
		if (bConvex)
			filename += "_convex.pxm";
		else
		{
			if (bFlip)
				filename += "_flipped";
			filename += "_tri.pmx";
		}

		const Path filepath = Project::GetCachePath() / "PhysX" / filename;
		if (bInvalidateOld)
		{
			bool removedCached = std::filesystem::remove(filepath);
			if (!removedCached)
				EG_CORE_ERROR("[Physics Engine] Couldn't delete cached collider data: '{0}'", filepath.u8string());
		}

		CookingResult result = CookingResult::Failure;
		if (!std::filesystem::exists(filepath))
		{
			result = bConvex ? CookConvexMesh(collisionMesh, outData) : CookTriangleMesh(collisionMesh, bFlip, outData);

			if (result == CookingResult::Success)
			{
				uint32_t bufferSize = sizeof(uint32_t) + outData.Size;
				ScopedDataBuffer colliderBuffer;
				colliderBuffer.Allocate(bufferSize);
				colliderBuffer.Write((const void*)&outData.Size, sizeof(uint32_t));
				colliderBuffer.Write(outData.Data, outData.Size, sizeof(uint32_t));
				bool bSuccessWrite = FileSystem::Write(filepath, colliderBuffer.GetDataBuffer());

				if (!bSuccessWrite)
					EG_CORE_ERROR("[Physics Engine] Failed to write collider to '{0}'", filepath.u8string());
			}
		}
		else
		{
			ScopedDataBuffer colliderBuffer(FileSystem::Read(filepath));
			if (colliderBuffer.Size() > 0)
			{
				outData.Size = colliderBuffer.Read<uint32_t>();
				outData.Data = colliderBuffer.ReadBytes(outData.Size, sizeof(uint32_t));

				result = CookingResult::Success;
			}
		}

		return result;
	}

	CookingResult PhysXCookingFactory::CookConvexMesh(const Ref<StaticMesh>& mesh, MeshColliderData& outData)
	{
		const auto& vertices = mesh->GetVertices();
		const auto& indices = mesh->GetIndeces();

		physx::PxConvexMeshDesc convexDesc;
		convexDesc.points.count = (uint32_t)vertices.size();
		convexDesc.points.stride = sizeof(Vertex);
		convexDesc.points.data = &vertices[0];
		convexDesc.indices.count = (uint32_t)indices.size() / 3;
		convexDesc.indices.stride = sizeof(uint32_t) * 3;
		convexDesc.indices.data = &indices[0];
		convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eSHIFT_VERTICES;

		physx::PxDefaultMemoryOutputStream buf;
		physx::PxConvexMeshCookingResult::Enum result;
		if (!s_CookingData->CookingSDK->cookConvexMesh(convexDesc, buf, &result))
		{
			EG_CORE_ERROR("[Physics Engine] Failed to cook convex mesh '{0}'", mesh->GetPath());
			return PhysXUtils::FromPhysXCookingResult(result);
		}

		outData.Size = buf.getSize();
		outData.Data = new uint8_t[outData.Size];
		memcpy(outData.Data, buf.getData(), outData.Size);

		return CookingResult::Success;
	}

	CookingResult PhysXCookingFactory::CookTriangleMesh(const Ref<StaticMesh>& mesh, bool bFlipNormals, MeshColliderData& outData)
	{
		const auto& vertices = mesh->GetVertices();
		const auto& indices = mesh->GetIndeces();

		physx::PxTriangleMeshDesc triangleDesc;
		triangleDesc.points.count = (uint32_t)vertices.size();
		triangleDesc.points.stride = sizeof(Vertex);
		triangleDesc.points.data = &vertices[0];
		triangleDesc.triangles.count = (uint32_t)indices.size() / 3;
		triangleDesc.triangles.stride = sizeof(uint32_t) * 3;
		triangleDesc.triangles.data = &indices[0];
		if (bFlipNormals)
			triangleDesc.flags |= physx::PxMeshFlag::eFLIPNORMALS;

		#if 0
				bool bValid = s_CookingData->CookingSDK->validateTriangleMesh(triangleDesc);
				if (!bValid)
				{
					EG_CORE_ERROR("[Physics Engine] Failed to validate triangle mesh '{0}'", mesh->GetPath());
					return CookingResult::Failure;
				}
		#endif

		physx::PxDefaultMemoryOutputStream buf;
		physx::PxTriangleMeshCookingResult::Enum result;
		if (!s_CookingData->CookingSDK->cookTriangleMesh(triangleDesc, buf, &result))
		{
			EG_CORE_ERROR("[Physics Engine] Failed to cook triangle mesh '{0}'", mesh->GetPath());
			return PhysXUtils::FromPhysXCookingResult(result);
		}

		outData.Size = buf.getSize();
		outData.Data = new uint8_t[outData.Size];
		memcpy(outData.Data, buf.getData(), outData.Size);

		return CookingResult::Success;
	}	
}
