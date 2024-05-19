#include "egpch.h"
#include "SkeletalMesh.h"

namespace Eagle
{
	Ref<SkeletalMesh> SkeletalMesh::Create(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal, const AABB& aabb)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal, const AABB& aabb)
				: SkeletalMesh(vertices, indices, skeletal, aabb) {}
		};

		return MakeRef<LocalSkeletalMesh>(vertices, indices, skeletal, aabb);
	}

	Ref<SkeletalMesh> SkeletalMesh::Create(const Ref<SkeletalMesh>& other)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const SkeletalMesh& other)
				: SkeletalMesh(other) {}
		};

		return MakeRef<LocalSkeletalMesh>(*other.get());
	}
}
