#include "egpch.h"
#include "SkeletalMesh.h"

namespace Eagle
{
	Ref<SkeletalMesh> SkeletalMesh::Create(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal)
	{
		class LocalSkeletalMesh : public SkeletalMesh
		{
		public:
			LocalSkeletalMesh(const std::vector<SkeletalVertex>& vertices, const std::vector<Index>& indices, const SkeletalMeshInfo& skeletal)
				: SkeletalMesh(vertices, indices, skeletal) {}
		};

		return MakeRef<LocalSkeletalMesh>(vertices, indices, skeletal);
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
