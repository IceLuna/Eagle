#include "egpch.h"
#include "StaticMesh.h"

#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	StaticMesh::~StaticMesh()
	{
		for (uint32_t i = 0; i < m_MaterialSlots; ++i)
		{
			if (m_Materials[i])
			{
				m_Materials[i]->RemoveOnAssetModifiedCallback(m_MaterialsCallbacks[i]);
			}
		}
	}

	void StaticMesh::SetMaterialAsset(uint32_t index, const Ref<AssetMaterial>& material)
	{
		if (m_Materials[index])
		{
			m_Materials[index]->RemoveOnAssetModifiedCallback(m_MaterialsCallbacks[index]);
		}
		m_Materials[index] = material;
		if (material)
		{
			material->AddOnAssetModifiedCallback(m_MaterialsCallbacks[index], [this]()
			{
				OnMaterialPropertyModified();
			});
		}
	}

	void StaticMesh::AddOnMaterialPropertyModifiedCallback(const GUID& id, const std::function<void()>& func)
	{
		m_Callbacks[id] = func;
	}

	void StaticMesh::RemoveOnMaterialPropertyModifiedCallback(const GUID& id)
	{
		m_Callbacks.erase(id);
	}

	Ref<StaticMesh> StaticMesh::Create(const std::vector<Vertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const AABB& aabb)
	{
		class LocalStaticMesh : public StaticMesh
		{
		public:
			LocalStaticMesh(const std::vector<Vertex>& vertices, const std::vector<std::vector<Index>>& indicesPerMaterial, const AABB& aabb)
				: StaticMesh(vertices, indicesPerMaterial, aabb) {}
		};

		return MakeRef<LocalStaticMesh>(vertices, indicesPerMaterial, aabb);
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
	
	void StaticMesh::OnMaterialPropertyModified()
	{
		for (auto& [_, func] : m_Callbacks)
			func();
	}
}
