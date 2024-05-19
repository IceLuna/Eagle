#include "egpch.h"
#include "StaticMesh.h"

namespace Eagle
{
	Ref<StaticMesh> StaticMesh::Create(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const AABB& aabb)
	{
		class LocalStaticMesh : public StaticMesh
		{
		public:
			LocalStaticMesh(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const AABB& aabb)
				: StaticMesh(vertices, indices, aabb) {}
		};

		return MakeRef<LocalStaticMesh>(vertices, indices, aabb);
	}

	Ref<StaticMesh> StaticMesh::Create(const Ref<StaticMesh>& other)
	{
		class LocalStaticMesh : public StaticMesh
		{
		public:
			LocalStaticMesh(const StaticMesh& other)
				: StaticMesh(other) {}
		};

		return MakeRef<LocalStaticMesh>(*other.get());
	}
}
