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
	};

	class GUID;

	class StaticMesh
	{
	public:
		//Use StaticMesh::Create() function
		StaticMesh() : Material(Material::Create())
		{
		}

		//Use StaticMesh::Create() function
		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: Material(Material::Create())
		, m_Vertices(vertices)
		, m_Indices(indices) 
		{
		}

		StaticMesh(const StaticMesh& other)
		: Material(Material::Create(other.Material))
		, m_GUID(other.m_GUID)
		, m_Vertices(other.m_Vertices)
		, m_Indices(other.m_Indices)
		, m_Path(other.m_Path)
		, m_AssetName(other.m_AssetName)
		, m_Index(other.m_Index)
		, bMadeOfMultipleMeshes(other.bMadeOfMultipleMeshes)
		{}

		const uint32_t* GetIndecesData() const { return m_Indices.data(); }
		const std::vector<uint32_t>& GetIndeces() const { return m_Indices; }
		uint32_t GetIndecesCount() const { return (uint32_t)m_Indices.size(); }

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		uint32_t GetVerticesCount() const { return (uint32_t)m_Vertices.size(); }

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
		static Ref<StaticMesh> Create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
		static Ref<StaticMesh> Create(const Ref<StaticMesh>& other);

	public:
		Ref<Eagle::Material> Material;
	private:
		GUID m_GUID;
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		Path m_Path;
		std::string m_AssetName = "None";
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
