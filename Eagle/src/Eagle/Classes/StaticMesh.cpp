#include "egpch.h"
#include "StaticMesh.h"

namespace Eagle
{
	Ref<StaticMesh> StaticMesh::Create(const std::vector<Vertex>& vertices, const std::vector<Index>& indices)
	{
		class LocalStaticMesh : public StaticMesh
		{
		public:
			LocalStaticMesh(const std::vector<Vertex>& vertices, const std::vector<Index>& indices)
				: StaticMesh(vertices, indices) {}
		};

		return MakeRef<LocalStaticMesh>(vertices, indices);
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
