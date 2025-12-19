#pragma once

#include "Eagle/Asset/AssetImporter.h"

namespace Eagle
{
	class TextureImporterPanel
	{
	public:
		TextureImporterPanel() = default;
		TextureImporterPanel(const std::vector<Path>& paths);

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		// @bOverride. Can be set to nullptr to not display it
		void Render2DSettings(const std::string& path, AssetImportTexture2DSettings& settings, glm::ivec2 size, bool bDrawingCommon, bool* bOverride = nullptr);
		void RenderCubeSettings(const std::string& path, AssetImportTextureCubeSettings& settings, glm::ivec2 size, bool* bOverride = nullptr);

	private:
		struct Texture2DData
		{
			AssetImportTexture2DSettings Settings;
			std::string AssetPath;
			glm::ivec2 Size = glm::ivec2(0);
			bool bOverride = false;
		};
		struct TextureCubeData
		{
			AssetImportTextureCubeSettings Settings;
			std::string AssetPath;
			glm::ivec2 Size = glm::ivec2(0);
			bool bOverride = false;
		};

		AssetImportTexture2DSettings m_Common2DSettings;
		AssetImportTextureCubeSettings m_CommonCubeSettings;

		std::string m_WindowName;
		std::vector<Texture2DData> m_2DTextures;
		std::vector<TextureCubeData> m_CubeTextures;
	};

	class MeshImporterPanel
	{
	public:
		MeshImporterPanel() = default;
		MeshImporterPanel(const std::vector<Path>& paths);

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		// @bOverride. Can be set to nullptr to not display it
		void RenderSettings(const std::string& path, AssetImportSettings& settings, bool& bSkeletal, bool* bOverride = nullptr);

	private:
		struct MeshData
		{
			AssetImportSettings Settings;
			std::string AssetPath;
			bool bSkeletal = false;
			bool bOverride = false;
		};

		AssetImportSettings m_CommonSettings;
		std::vector<MeshData> m_Meshes;
		bool m_AsSkeletal = false;
	};

	class AnimationGraphImporterPanel
	{
	public:
		AnimationGraphImporterPanel() = default;
		AnimationGraphImporterPanel(const Path& path) : m_Path(path) {}

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		Ref<AssetSkeletalMesh> m_Mesh;

		Path m_Path;
	};

	class AnimationBlendSpaceImporterPanel
	{
	public:
		AnimationBlendSpaceImporterPanel() = default;
		AnimationBlendSpaceImporterPanel(const Path& path) : m_Path(path) {}

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		Ref<AssetSkeletalMesh> m_Mesh;

		Path m_Path;
	};
}
