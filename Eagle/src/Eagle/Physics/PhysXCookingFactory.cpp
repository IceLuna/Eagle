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
		EG_CORE_ASSERT(!s_CookingData, "Trying to init Cooking Factory twice!");

		s_CookingData = new PhysXCookingData(PhysXInternal::GetPhysics().getTolerancesScale());
		s_CookingData->CookingParams.meshWeldTolerance = 0.1f;
		s_CookingData->CookingParams.meshPreprocessParams = physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
		s_CookingData->CookingParams.midphaseDesc = physx::PxMeshMidPhase::eBVH34;

		s_CookingData->CookingSDK = PxCreateCooking(PX_PHYSICS_VERSION, PhysXInternal::GetFoundation(), s_CookingData->CookingParams);
		EG_CORE_ASSERT(s_CookingData->CookingSDK, "Failed to create Cooking");
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
		if (!component.CollisionMesh)
		{
			EG_CORE_ERROR("[Physics Engine] Cooking: Invalid mesh!");
			return CookingResult::Failure;
		}

		CookingResult result = CookingResult::Failure;
		std::filesystem::path filepath = Project::GetCachePath() / "PhysX" /
										(component.CollisionMesh->GetPath().stem().u8string() + "_" + component.CollisionMesh->GetName() +
										(component.IsConvex ? "_convex.pxm" : "_tri.pmx"));

		if (bInvalidateOld)
		{
			bool removedCached = std::filesystem::remove(filepath);
			if (!removedCached)
				EG_CORE_ERROR("Couldn't delete cached collider data: '{0}'", filepath.u8string());
		}

		if (!std::filesystem::exists(filepath))
		{
			result = component.IsConvex ? CookConvexMesh(component.CollisionMesh, outData) : CookTriangleMesh(component.CollisionMesh, outData);

			if (result == CookingResult::Success)
			{
				uint32_t bufferSize = sizeof(uint32_t) + outData.Size;
				DataBuffer colliderBuffer;
				colliderBuffer.Allocate(bufferSize);
				colliderBuffer.Write((const void*)&outData.Size, sizeof(uint32_t));
				colliderBuffer.Write(outData.Data, outData.Size, sizeof(uint32_t));
				bool bSuccessWrite = FileSystem::Write(filepath, colliderBuffer);
				colliderBuffer.Release();

				if (!bSuccessWrite)
					EG_CORE_ERROR("Failed to write collider to '{0}'", filepath.u8string());
			}
		}
		else
		{
			DataBuffer colliderBuffer = FileSystem::Read(filepath);
			if (colliderBuffer.GetSize() > 0)
			{
				outData.Size = colliderBuffer.Read<uint32_t>();
				outData.Data = colliderBuffer.ReadBytes(outData.Size, sizeof(uint32_t));

				colliderBuffer.Release();
				result = CookingResult::Success;
			}
		}

		if (result == CookingResult::Success)
		{
			GenerateDebugMesh(component, outData);
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

		#if 0
			bool bValid = s_CookingData->CookingSDK->validateConvexMesh(convexDesc);
			if (!bValid)
			{
				EG_CORE_ERROR("[Physics Engine] Failed to validate convex mesh '{0}'", mesh->GetPath());
				return CookingResult::Failure;
			}
		#endif

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

	CookingResult PhysXCookingFactory::CookTriangleMesh(const Ref<StaticMesh>& mesh, MeshColliderData& outData)
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

	void PhysXCookingFactory::GenerateDebugMesh(MeshColliderComponent& component, const MeshColliderData& colliderData)
	{
		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		if (component.IsConvex)
		{
			physx::PxConvexMesh* convexMesh = PhysXInternal::GetPhysics().createConvexMesh(input);

			vertices.reserve(100);
			indices.reserve(100);

			const uint32_t nbPolygons = convexMesh->getNbPolygons();
			const physx::PxVec3* convexVertices = convexMesh->getVertices();
			const physx::PxU8* convexIndices = convexMesh->getIndexBuffer();

			uint32_t nbVertices = 0;
			uint32_t nbFaces = 0;
			uint32_t vertCounter = 0;

			for (uint32_t i = 0; i < nbPolygons; ++i)
			{
				physx::PxHullPolygon polygon;
				convexMesh->getPolygonData(i, polygon);
				nbVertices += polygon.mNbVerts;
				nbFaces += (polygon.mNbVerts - 2) * 3;

				physx::PxVec3 normal(polygon.mPlane[0], polygon.mPlane[1], polygon.mPlane[2]);

				uint32_t vertexIndex0 = vertCounter;
				for (uint32_t vI = 0; vI < polygon.mNbVerts; ++vI)
				{
					Vertex& vertex = vertices.emplace_back();
					vertex.Position = PhysXUtils::FromPhysXVector(convexVertices[convexIndices[polygon.mIndexBase + vI]]);
					vertex.Normal = PhysXUtils::FromPhysXVector(normal);
					vertCounter++;
				}

				for (uint32_t vI = 1; vI < uint32_t(polygon.mNbVerts) - 1; ++vI)
				{
					indices.emplace_back(vertexIndex0);
					indices.emplace_back(vertexIndex0 + vI + 1);
					indices.emplace_back(vertexIndex0 + vI);
				}
			}
			
			convexMesh->release();
		}
		else
		{
			physx::PxTriangleMesh* triMesh = PhysXInternal::GetPhysics().createTriangleMesh(input);

			const uint32_t nbVertices = triMesh->getNbVertices();
			const physx::PxVec3* triVertices = triMesh->getVertices();
			const uint32_t nbTriangles = triMesh->getNbTriangles();
			auto meshFlag = triMesh->getTriangleMeshFlags();
			const bool bU16Indices = meshFlag & physx::PxTriangleMeshFlag::e16_BIT_INDICES;

			vertices.reserve(nbVertices);
			indices.reserve((size_t)nbTriangles);

			for (uint32_t v = 0; v < nbVertices; ++v)
			{
				Vertex& vertex = vertices.emplace_back();
				vertex.Position = PhysXUtils::FromPhysXVector(triVertices[v]);
			}

			if (bU16Indices)
			{
				const physx::PxU16* triangles = (const physx::PxU16*)triMesh->getTriangles();
				for (uint32_t tri = 0; tri < nbTriangles; ++tri)
				{
					indices.emplace_back(triangles[3 * tri + 0]);
					indices.emplace_back(triangles[3 * tri + 1]);
					indices.emplace_back(triangles[3 * tri + 2]);
				}
			}
			else
			{
				const physx::PxU32* triangles = (const physx::PxU32*)triMesh->getTriangles();
				for (uint32_t tri = 0; tri < nbTriangles; ++tri)
				{
					indices.emplace_back(triangles[3 * tri + 0]);
					indices.emplace_back(triangles[3 * tri + 1]);
					indices.emplace_back(triangles[3 * tri + 2]);
				}
			}

			triMesh->release();
		}
	
		component.DebugMesh = StaticMesh::Create(vertices, indices);
	}
	
}
