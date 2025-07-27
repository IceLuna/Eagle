#include "egpch.h"
#include "PhysicsUtils.h"
#include "PhysicsEngine.h"
#include "PhysicsActor.h"
#include "PhysicsRagdollActor.h"
#include "Eagle/Math/Math.h"

namespace Eagle
{
	static physx::PxQueryFlags ToPxQueryFlags(PhysicsQueryType type)
	{
		using namespace physx;
		PxQueryFlags result = physx::PxQueryFlag::ePREFILTER;

		if (HasFlags(type, PhysicsQueryType::Static))
			result |= PxQueryFlag::eSTATIC;
		if (HasFlags(type, PhysicsQueryType::Dynamic))
			result |= PxQueryFlag::eDYNAMIC;
		if (HasFlags(type, PhysicsQueryType::AnyHit))
			result |= PxQueryFlag::eANY_HIT;

		return result;
	}

	CookingResult PhysXUtils::FromPhysXCookingResult(physx::PxConvexMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
			case physx::PxConvexMeshCookingResult::eSUCCESS: return CookingResult::Success;
			case physx::PxConvexMeshCookingResult::eZERO_AREA_TEST_FAILED: return CookingResult::ZeroAreaTestFailed;
			case physx::PxConvexMeshCookingResult::ePOLYGONS_LIMIT_REACHED: return CookingResult::PolygonLimitReached;
			case physx::PxConvexMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	CookingResult PhysXUtils::FromPhysXCookingResult(physx::PxTriangleMeshCookingResult::Enum cookingResult)
	{
		switch (cookingResult)
		{
		case physx::PxTriangleMeshCookingResult::eSUCCESS: return CookingResult::Success;
		case physx::PxTriangleMeshCookingResult::eLARGE_TRIANGLE: return CookingResult::LargeTriangle;
		case physx::PxTriangleMeshCookingResult::eFAILURE: return CookingResult::Failure;
		}

		return CookingResult::Failure;
	}

	physx::PxTransform PhysXUtils::ToPhysXTranform(const glm::mat4& transform)
	{
		glm::vec3 location, rotation, scale;
		Math::DecomposeTransformMatrix(transform, location, rotation, scale);
		
		physx::PxVec3 p = ToPhysXVector(location);
		physx::PxQuat q = ToPhysXQuat(glm::quat(rotation));
		
		return physx::PxTransform(p, q);
	}

	physx::PxTransform PhysXUtils::ToPhysXTranform(const Transform& transform)
	{
		return ToPhysXTranform(transform.Location, transform.Rotation);
	}
	
	physx::PxTransform PhysXUtils::ToPhysXTranform(const glm::vec3& location, const Rotator& rotation)
	{
		return physx::PxTransform(ToPhysXVector(location), ToPhysXQuat(rotation));
	}

	physx::PxTransform PhysXUtils::ToPhysXTranform(const glm::vec3& location)
	{
		return physx::PxTransform(ToPhysXVector(location));
	}
	
	physx::PxBroadPhaseType::Enum PhysXUtils::ToPhysXBroadphaseType(BroadphaseType type)
	{
		switch (type)
		{
			case BroadphaseType::SweepAndPrune:		return physx::PxBroadPhaseType::Enum::eSAP;
			case BroadphaseType::MultiBoxPrune:		return physx::PxBroadPhaseType::Enum::eMBP;
			case BroadphaseType::AutomaticBoxPrune: return physx::PxBroadPhaseType::Enum::eABP;
			default: return physx::PxBroadPhaseType::Enum::eABP;
		}
	}
	
	physx::PxFrictionType::Enum PhysXUtils::ToPhysXFrictionType(FrictionType type)
	{
		switch (type)
		{
			case FrictionType::Patch:			return physx::PxFrictionType::Enum::ePATCH;
			case FrictionType::OneDirectional:	return physx::PxFrictionType::Enum::eONE_DIRECTIONAL;
			case FrictionType::TwoDirectional:	return physx::PxFrictionType::Enum::eTWO_DIRECTIONAL;
			default: return physx::PxFrictionType::Enum::eONE_DIRECTIONAL;
		}
	}

	physx::PxFilterData PhysXUtils::GetPxFilterData(CollisionGroup group, CollisionGroup interactingGroup, CollisionDetectionType collisionDetection)
	{
		physx::PxFilterData filterData;
		filterData.word0 = uint32_t(group);
		filterData.word1 = uint32_t(interactingGroup);
		filterData.word2 = uint32_t(collisionDetection);
		filterData.word3 = 0u;
		return filterData;
	}

	physx::PxQueryFilterData PhysXUtils::GetPxQueryFilterData(PhysicsQueryType type)
	{
		physx::PxQueryFilterData result;
		result.flags = ToPxQueryFlags(type);
		return result;
	}

	void PhysXUtils::GetBoxGeometry(const physx::PxBoxGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices)
	{
		constexpr size_t numVertices = 8;
		vertices.reserve(numVertices);

		const glm::vec3 extents = FromPhysXVector(geometry.halfExtents);

		vertices.push_back(glm::vec3(-extents.x, -extents.y, -extents.z));
		vertices.push_back(glm::vec3(extents.x, -extents.y, -extents.z));
		vertices.push_back(glm::vec3(extents.x, extents.y, -extents.z));
		vertices.push_back(glm::vec3(-extents.x, extents.y, -extents.z));

		vertices.push_back(glm::vec3(-extents.x, -extents.y, extents.z));
		vertices.push_back(glm::vec3(extents.x, -extents.y, extents.z));
		vertices.push_back(glm::vec3(extents.x, extents.y, extents.z));
		vertices.push_back(glm::vec3(-extents.x, extents.y, extents.z));

		constexpr size_t numIndices = 36;
		constexpr uint32_t boxIndices[numIndices] =
		{
			2, 1, 0,
			0, 3, 2,
			3, 0, 7,
			0, 4, 7,
			0, 1, 5,
			0, 5, 4,
			1, 2, 5,
			6, 5, 2,
			7, 2, 3,
			7, 6, 2,
			7, 4, 5,
			7, 5, 6
		};
		indices.reserve(numIndices);
		for (int i = 0; i < numIndices; ++i)
		{
			indices.push_back(boxIndices[i]);
		}
	}

	void PhysXUtils::GetCapsuleGeometry(const physx::PxCapsuleGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const uint32_t stacks, const uint32_t slices)
	{
		const glm::vec3 base(-geometry.halfHeight, 0.f, 0.f);
		const glm::vec3 top(geometry.halfHeight, 0.f, 0.f);
		const float radius = geometry.radius;

		// topStack refers to the top row of vertices starting at 0
		// get an even number so our caps reach all the way out to sphere radius
		const uint32_t topStack = stacks % 2 ? stacks + 1 : stacks;
		const uint32_t midStack = topStack / 2;

		vertices.reserve(slices * topStack + 2);
		indices.reserve((slices - 1) * topStack * 6);

		const float thetaFactor = 1.f / float(topStack) * physx::PxPi;
		const float phiFactor = 1.f / float(slices - 1) * physx::PxTwoPi;

		// bottom cap
		vertices.push_back(base + glm::vec3(-radius, 0.f, 0.f));
		for (size_t stack = 1; stack <= midStack; ++stack)
		{
			for (size_t i = 0; i < slices; ++i)
			{
				float theta = float(stack) * thetaFactor;
				float phi = float(i) * phiFactor;

				float sinTheta = glm::sin(theta);
				float cosTheta = glm::cos(theta);

				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				vertices.push_back(base + glm::vec3(-cosTheta * radius, sinTheta * sinPhi * radius, sinTheta * cosPhi * radius));
			}
		}

		// top cap
		for (size_t stack = midStack; stack < topStack; ++stack)
		{
			for (size_t i = 0; i < slices; ++i)
			{
				float theta = float(stack) * thetaFactor;
				float phi = float(i) * phiFactor;

				float sinTheta = glm::sin(theta);
				float cosTheta = glm::cos(theta);

				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				vertices.push_back(top + glm::vec3(-cosTheta * radius, sinTheta * sinPhi * radius, sinTheta * cosPhi * radius));
			}
		}
		vertices.push_back(top + glm::vec3(radius, 0.f, 0.f));

		const uint32_t lastVertex = uint32_t(vertices.size()) - 1;
		const uint32_t topRow = uint32_t(vertices.size()) - slices - 1;

		// top and bottom segment indices
		for (uint32_t i = 0; i < slices - 1; ++i)
		{
			// bottom (add one to account for single bottom vertex)
			indices.push_back(0);
			indices.push_back(i + 2);
			indices.push_back(i + 1);

			//top (topRow accounts for the added bottom vertex)
			indices.push_back(topRow + i + 0);
			indices.push_back(topRow + i + 1);
			indices.push_back(lastVertex);
		}

		// there are stacks + 1 stacks because we stretched the middle for the cylinder section,
		// but we already built the top and bottom stack so there are stacks + 1 - 2 to build
		// add 1 to each vertex index because there is a single bottom vertex for the bottom cap
		for (uint32_t j = 0; j < stacks - 1; ++j)
		{
			for (uint32_t i = 0; i < slices - 1; ++i)
			{
				indices.push_back(j * slices + i + 2);
				indices.push_back((j + 1) * slices + i + 2);
				indices.push_back((j + 1) * slices + i + 1);
				indices.push_back(j * slices + i + 1);
				indices.push_back(j * slices + i + 2);
				indices.push_back((j + 1) * slices + i + 1);
			}
		}
	}

	void PhysXUtils::GetConvexMeshGeometry(const physx::PxConvexMeshGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices)
	{
		const physx::PxConvexMesh* convexMesh = geometry.convexMesh;
		const physx::PxU8* pxIndices = convexMesh->getIndexBuffer();
		const physx::PxVec3* pxVertices = convexMesh->getVertices();
		const uint32_t numPolys = convexMesh->getNbPolygons();
		uint32_t currentIndex = 0;
		physx::PxHullPolygon poly;

		// Reserve data
		{
			size_t verticesCount = 0;
			for (uint32_t polygonIndex = 0; polygonIndex < numPolys; ++polygonIndex)
			{
				if (convexMesh->getPolygonData(polygonIndex, poly))
				{
					const uint32_t triangleCount = poly.mNbVerts - 2;
					verticesCount += triangleCount * 3u;
				}
			}
			vertices.reserve(verticesCount);
			indices.reserve(verticesCount);
		}

		for (uint32_t polygonIndex = 0; polygonIndex < numPolys; ++polygonIndex)
		{
			if (convexMesh->getPolygonData(polygonIndex, poly))
			{
				constexpr uint32_t index1 = 0;
				uint32_t index2 = 1;
				uint32_t index3 = 2;

				const glm::vec3 a = FromPhysXVector(geometry.scale.transform(pxVertices[pxIndices[poly.mIndexBase + index1]]));
				const uint32_t triangleCount = poly.mNbVerts - 2;

				for (uint32_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
				{
					EG_CORE_ASSERT(index3 < poly.mNbVerts, "Implementation error: attempted to index outside range of polygon vertices.");

					const glm::vec3 b = FromPhysXVector(geometry.scale.transform(pxVertices[pxIndices[poly.mIndexBase + index2]]));
					const glm::vec3 c = FromPhysXVector(geometry.scale.transform(pxVertices[pxIndices[poly.mIndexBase + index3]]));

					vertices.push_back(a);
					vertices.push_back(b);
					vertices.push_back(c);
					indices.push_back(currentIndex++);
					indices.push_back(currentIndex++);
					indices.push_back(currentIndex++);

					index2 = index3++;
				}
			}
		}
	}

	void PhysXUtils::GetHeightFieldGeometry(const physx::PxHeightFieldGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const AABB* optionalBounds)
	{
		int minX = 0;
		int minY = 0;

		// rows map to y and columns to x see EditorTerrainComponent
		int maxX = geometry.heightField->getNbColumns() - 1;
		int maxY = geometry.heightField->getNbRows() - 1;

		if (optionalBounds)
		{
			// convert the provided bounds to heightfield sample grid positions
			const AABB& bounds = *optionalBounds;
			const float inverseRowScale = 1.f / geometry.rowScale;
			const float inverseColumnScale = 1.f / geometry.columnScale;

			minX = glm::max(minX, int(floor(bounds.Min.x * inverseColumnScale)));
			minY = glm::max(minY, int(floor(bounds.Min.y * inverseRowScale)));
			maxX = glm::min(maxX, int(ceil(bounds.Max.x * inverseColumnScale)));
			maxY = glm::min(maxY, int(ceil(bounds.Max.y * inverseRowScale)));

			// Make sure min values don't exceed the max 
			minX = glm::min(minX, maxX);
			minY = glm::min(minY, maxY);
		}

		// num quads * 2 triangles per quad * 3 vertices per triangle
		const size_t numVertices = (maxY - minY) * (maxX - minX) * 2 * 3;
		vertices.reserve(numVertices);

		for (int y = minY; y < maxY; ++y)
		{
			for (int x = minX; x < maxX; ++x)
			{
				const physx::PxHeightFieldSample& pxSample = geometry.heightField->getSample(y, x);

				if (pxSample.materialIndex0 == physx::PxHeightFieldMaterial::eHOLE ||
					pxSample.materialIndex1 == physx::PxHeightFieldMaterial::eHOLE)
				{
					// skip terrain geometry marked as eHOLE, this feature is often used for tunnels
					continue;
				}

				float height = float(pxSample.height) * geometry.heightScale;

				const glm::vec3 v0(float(x) * geometry.rowScale, float(y) * geometry.columnScale, height);

				height = float(geometry.heightField->getSample(y + 1, x).height) * geometry.heightScale;
				const glm::vec3 v1(float(x) * geometry.rowScale, float(y + 1) * geometry.columnScale, height);

				height = float(geometry.heightField->getSample(y, x + 1).height) * geometry.heightScale;
				const glm::vec3 v2(float(x + 1) * geometry.rowScale, float(y) * geometry.columnScale, height);

				height = float(geometry.heightField->getSample(y + 1, x + 1).height) * geometry.heightScale;
				const glm::vec3 v3(float(x + 1) * geometry.rowScale, float(y + 1) * geometry.columnScale, height);

				vertices.push_back(v0);
				vertices.push_back(v1);
				vertices.push_back(v2);

				vertices.push_back(v1);
				vertices.push_back(v3);
				vertices.push_back(v2);
			}
		}
	}

	void PhysXUtils::GetSphereGeometry(const physx::PxSphereGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices, const uint32_t stacks, const uint32_t slices)
	{
		const float radius = geometry.radius;
		const size_t vertexCount = slices * (stacks - 2) + 2;
		vertices.reserve(vertexCount);

		vertices.push_back(glm::vec3(0.f, 0.f, radius));
		vertices.push_back(glm::vec3(0.f, 0.f, -radius));

		for (size_t j = 1; j < stacks - 1; ++j)
		{
			for (size_t i = 0; i < slices; ++i)
			{
				float theta = (j / (float)(stacks - 1)) * physx::PxPi;
				float phi = (i / (float)(slices - 1)) * physx::PxTwoPi;

				float sinTheta = glm::sin(theta);
				float cosTheta = glm::cos(theta);

				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				vertices.push_back(glm::vec3(sinTheta * cosPhi * radius, -sinTheta * sinPhi * radius, cosTheta * radius));
			}
		}

		const size_t indexCount = (slices - 1) * (stacks - 2) * 6;
		indices.reserve(indexCount);

		for (uint32_t i = 0; i < slices - 1; ++i)
		{
			indices.push_back(0);
			indices.push_back(i + 2);
			indices.push_back(i + 3);

			indices.push_back((stacks - 3) * slices + i + 3);
			indices.push_back((stacks - 3) * slices + i + 2);
			indices.push_back(1);
		}

		for (uint32_t j = 0; j < stacks - 3; ++j)
		{
			for (uint32_t i = 0; i < slices - 1; ++i)
			{
				indices.push_back((j + 1) * slices + i + 3);
				indices.push_back(j * slices + i + 3);
				indices.push_back((j + 1) * slices + i + 2);
				indices.push_back(j * slices + i + 3);
				indices.push_back(j * slices + i + 2);
				indices.push_back((j + 1) * slices + i + 2);
			}
		}
	}

	void PhysXUtils::GetTriangleMeshGeometry(const physx::PxTriangleMeshGeometry& geometry, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices)
	{
		const physx::PxTriangleMesh* triangleMesh = geometry.triangleMesh;
		const physx::PxVec3* meshVertices = triangleMesh->getVertices();
		const uint32_t vertCount = triangleMesh->getNbVertices();
		const uint32_t triangleCount = triangleMesh->getNbTriangles();

		vertices.reserve(vertCount);
		indices.reserve(triangleCount * 3);

		for (uint32_t vertIndex = 0; vertIndex < vertCount; ++vertIndex)
		{
			vertices.push_back(FromPhysXVector(geometry.scale.transform(meshVertices[vertIndex])));
		}

		physx::PxTriangleMeshFlags triangleMeshFlags = triangleMesh->getTriangleMeshFlags();
		if (triangleMeshFlags.isSet(physx::PxTriangleMeshFlag::Enum::e16_BIT_INDICES))
		{
			const physx::PxU16* triangles = static_cast<const physx::PxU16*>(triangleMesh->getTriangles());
			for (uint32_t triangleIndex = 0; triangleIndex < triangleCount * 3; triangleIndex += 3)
			{
				indices.push_back(triangles[triangleIndex]);
				indices.push_back(triangles[triangleIndex + 1]);
				indices.push_back(triangles[triangleIndex + 2]);
			}
		}
		else
		{
			const physx::PxU32* triangles = static_cast<const physx::PxU32*>(triangleMesh->getTriangles());
			for (uint32_t triangleIndex = 0; triangleIndex < triangleCount * 3; triangleIndex += 3)
			{
				indices.push_back(triangles[triangleIndex]);
				indices.push_back(triangles[triangleIndex + 1]);
				indices.push_back(triangles[triangleIndex + 2]);
			}
		}
	}

	static SceneQueryHit GetHitFromPxOverlapHit(const physx::PxOverlapHit& pxHit)
	{
		SceneQueryHit hit;
		if (pxHit.actor && pxHit.actor->userData)
		{
			if (pxHit.actor->userData)
			{
				const PhysicsActorBase* actor = (PhysicsActorBase*)pxHit.actor->userData;
				hit.EntityID = actor->GetEntity();
				hit.Body = actor->GetPhysXActor();
			}

			if (pxHit.shape != nullptr)
			{
				hit.Shape = (ColliderShape*)pxHit.shape->userData;
			}
		}
		return hit;
	}

	UnboundedOverlapCallback::UnboundedOverlapCallback(const UnboundedOverlapHitCallback& hitCallback, std::vector<physx::PxOverlapHit>& hitBuffer, QueryHits& hits)
		: m_hitCallback(hitCallback), m_results(hits), physx::PxHitCallback<physx::PxOverlapHit>(hitBuffer.data(), static_cast<physx::PxU32>(hitBuffer.size()))
	{

	}
	
	physx::PxAgain UnboundedOverlapCallback::processTouches(const physx::PxOverlapHit* buffer, physx::PxU32 numHits)
	{
		for (auto it = buffer; it != buffer + numHits; ++it)
		{
			const SceneQueryHit hit = GetHitFromPxOverlapHit(*it);
			if (hit.IsValid() && !m_hitCallback(std::optional<SceneQueryHit>(hit)))
			{
				return false;
			}
			m_results.emplace_back(hit);
		}
		return true;
	}

	inline uint64_t Combine(uint32_t word0, uint32_t word1)
	{
		return (uint64_t(word0) << 32) | word1;
	}
	
	physx::PxQueryHitType::Enum PhysXQueryFilterCallback::preFilter(const physx::PxFilterData& queryFilterData, const physx::PxShape* pxShape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryTypes)
	{
		if (m_IgnoredEntities && actor->userData)
		{
			PhysicsActor* myActor = (PhysicsActor*)actor->userData;
			if (m_IgnoredEntities->count(myActor->GetEntity()))
				return physx::PxQueryHitType::eNONE;
		}

		auto shapeFilterData = pxShape->getSimulationFilterData();
		if (m_CollisionGroupMask & shapeFilterData.word0)
			return m_HitType;

		return physx::PxQueryHitType::eNONE;
	}
}
