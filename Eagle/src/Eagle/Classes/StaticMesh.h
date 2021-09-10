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

	class StaticMesh
	{
	public:
		StaticMesh() : Material(Material::Create())
		{
			Material->Shader = ShaderLibrary::GetOrLoad("assets/shaders/MeshShader.glsl");
		}

		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: Material(Material::Create())
		, m_Vertices(vertices)
		, m_Indices(indices) 
		{
			Material->Shader = ShaderLibrary::GetOrLoad("assets/shaders/MeshShader.glsl");
		}

		const uint32_t* GetIndecesData() const { return m_Indices.data(); }
		const std::vector<uint32_t>& GetIndeces() const { return m_Indices; }
		uint32_t GetIndecesCount() const { return (uint32_t)m_Indices.size(); }

		const Vertex* GetVerticesData() const { return m_Vertices.data(); }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		uint32_t GetVerticesCount() const { return (uint32_t)m_Vertices.size(); }

		const std::filesystem::path& GetPath() const { return m_Path; }
		const std::string& GetName() const { return m_AssetName; }
		bool IsMadeOfMultipleMeshes() const { return bMadeOfMultipleMeshes; }
		
		//Some 3D files can contain multiple meshes. If it does, meshes are assigned an index within 3D file.
		uint32_t GetIndex() const { return m_Index; }

	public:
		//*If bLazy is set to true, textures won't be loaded.
		//*If bForceImportingAsASingleMesh is set to true, in case there's multiple meshes in a file, MessageBox will not pop up asking if you want to import them as a single mesh
		//*If bAskQuestion is set to true, in case there's multiple meshes in a file and 'bForceImportingAsASingleMesh' is set to true, MessageBox will pop up asking if you want to import them as a single mesh
		static Ref<StaticMesh> Create(const std::filesystem::path& filename, bool bLazy = false, bool bForceImportingAsASingleMesh = false, bool bAskQuestion = true);

	public:
		Ref<Eagle::Material> Material;
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
		std::filesystem::path m_Path;
		std::string m_AssetName = "None";
		uint32_t m_Index = 0u;
		bool bMadeOfMultipleMeshes = false;
	};

	class StaticMeshLibrary
	{
	public:
		static void Add(const Ref<StaticMesh>& staticMesh) { m_Meshes.push_back(staticMesh); }
		static bool Get(const std::filesystem::path& path, Ref<StaticMesh>* staticMesh, uint32_t index = 0u);

		static const std::vector<Ref<StaticMesh>>& GetMeshes() { return m_Meshes; }

	private:
		StaticMeshLibrary() = default;
		StaticMeshLibrary(const StaticMeshLibrary&) = default;

		static std::vector<Ref<StaticMesh>> m_Meshes;
	};
}
