#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Eagle/Renderer/Material.h"

namespace Eagle
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec2 TexCoords;

		bool operator==(const Vertex& other) const
		{
			return Position == other.Position &&
				Normal == other.Normal &&
				Tangent == other.Tangent &&
				TexCoords == other.TexCoords;
		}

		bool operator!=(const Vertex& other) const
		{
			return !((*this) == other);
		}
	};

	using Index = uint32_t;

	class GUID;

	class StaticMesh
	{
	public:
		//Use StaticMesh::Create() function
		StaticMesh() : Material(Material::Create())
		{
		}

		//Use StaticMesh::Create() function
		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<Index>& indices)
		: Material(Material::Create())
		, m_Vertices(vertices)
		, m_Indices(indices) 
		{
		}

		StaticMesh(const StaticMesh& other)
		: Material(other.Material)
		, m_Vertices(other.m_Vertices)
		, m_Indices(other.m_Indices)
		, m_Path(other.m_Path)
		, m_AssetName(other.m_AssetName)
		, m_GUID(other.m_GUID)
		, m_Index(other.m_Index)
		, bMadeOfMultipleMeshes(other.bMadeOfMultipleMeshes)
		{}

		const Index* GetIndecesData() const { return m_Indices.data(); }
		const std::vector<Index>& GetIndeces() const { return m_Indices; }
		size_t GetIndecesCount() const { return m_Indices.size(); }

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		size_t GetVerticesCount() const { return m_Vertices.size(); }

		const Path& GetPath() const { return m_Path; }
		const std::string& GetName() const { return m_AssetName; }
		bool IsMadeOfMultipleMeshes() const { return bMadeOfMultipleMeshes; }
		bool IsValid() const { return m_Vertices.size() && m_Indices.size(); }
		const GUID& GetGUID() const { return m_GUID; }

		//Some 3D files can contain multiple meshes. If it does, meshes are assigned an index within 3D file.
		uint32_t GetIndex() const { return m_Index; }

	public:
		//*If bLazy is set to true, textures won't be loaded.
		//*If bForceImportingAsASingleMesh is set to true, in case there's multiple meshes in a file, MessageBox will not pop up asking if you want to import them as a single mesh
		//*If bAskQuestion is set to true, in case there's multiple meshes in a file and 'bForceImportingAsASingleMesh' is set to true, MessageBox will pop up asking if you want to import them as a single mesh
		static Ref<StaticMesh> Create(const Path& filename, bool bLazy = false, bool bForceImportingAsASingleMesh = false, bool bAskQuestion = true);
		static Ref<StaticMesh> Create(const std::vector<Vertex>& vertices, const std::vector<Index>& indices);
		static Ref<StaticMesh> Create(const Ref<StaticMesh>& other);

	public:
		Ref<Eagle::Material> Material;
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<Index> m_Indices;
		Path m_Path;
		std::string m_AssetName = "None";
		GUID m_GUID;
		uint32_t m_Index = 0u;
		bool bMadeOfMultipleMeshes = false;
	};

	class StaticMeshLibrary
	{
	public:
		static void Add(const Ref<StaticMesh>& staticMesh) { m_Meshes.push_back(staticMesh); }
		static bool Get(const Path& path, Ref<StaticMesh>* outStaticMesh, uint32_t index = 0u);
		static bool Exists(const Path& path);
		static bool Get(const GUID& guid, Ref<StaticMesh>* outStaticMesh, uint32_t index = 0u);
		static bool Exists(const GUID& guid);

		static const std::vector<Ref<StaticMesh>>& GetMeshes() { return m_Meshes; }

	private:
		StaticMeshLibrary() = default;
		StaticMeshLibrary(const StaticMeshLibrary&) = default;

		//TODO: Move to AssetManager::Shutdown()
		static void Clear() { m_Meshes.clear(); }
		friend class Renderer;

		static std::vector<Ref<StaticMesh>> m_Meshes;
	};
}
