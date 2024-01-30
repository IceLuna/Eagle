#pragma once

#include "Eagle/Asset/AssetImporter.h"

namespace Eagle
{
	class TextureImporterPanel
	{
	public:
		TextureImporterPanel() = default;
		TextureImporterPanel(const Path& path);

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		AssetImportTexture2DSettings m_2DSettings;
		AssetImportTextureCubeSettings m_CubeSettings;

		Path m_Path;
		bool bCube = false;
	};

	class MeshImporterPanel
	{
	public:
		MeshImporterPanel() = default;
		MeshImporterPanel(const Path& path) : m_Path(path) {}

		// @importTo. Destination folder
		// Returns true on success (if asset was created)
		bool OnImGuiRender(const Path& importTo, bool* pOpen);

	private:
		AssetImportMeshSettings m_Settings;

		Path m_Path;
	};
}
