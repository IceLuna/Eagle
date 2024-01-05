#pragma once

#include "Asset.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	struct AssetImportTexture2DSettings
	{
		AssetTexture2DFormat ImportFormat = AssetTexture2DFormat::Default;
		FilterMode FilterMode = FilterMode::Bilinear;
		AddressMode AddressMode = AddressMode::Wrap;
		float Anisotropy = 1.f;
		uint32_t MipsCount = 1;
	};

	struct AssetImportTextureCubeSettings
	{
		AssetTextureCubeFormat ImportFormat = AssetTextureCubeFormat::Default;
		uint32_t LayerSize = 512;
	};

	struct AssetImportMeshSettings
	{
		bool bImportAsSingleMesh = false;
	};

	struct AssetImportSettings
	{
		AssetImportTexture2DSettings Texture2DSettings;
		AssetImportTextureCubeSettings TextureCubeSettings;
		AssetImportMeshSettings MeshSettings;
	};

	// The purpose of this class is to take a path to a raw asset data (such as `.png`, `.fbx`, etc...)
	// and convert it into `.egasset` file format.
	class AssetImporter
	{
	public:
		// Returns true, if an eagle asset was successfully created
		// @pathToRaw - path to a raw asset (such as `png`, `.fbx`, etc...)
		// @saveTo - folder to save an imported asset to (must be somewhere within projects content folder)
		static bool Import(const Path& pathToRaw, const Path& saveTo, const AssetImportSettings& settings);

		// Since materials are not really imported, this function will just generate a material asset.
		// The final `filename` might be a bit different if the requested filename is already taken.
		// So the function returns the final path to the file it just created.
		// @saveTo - folder to save an imported asset to (must be somewhere within projects content folder)
		// @filename - filename without an extension
		static Path CreateMaterial(const Path& saveTo, const std::string& filename = "NewMaterial");
		static Path CreatePhysicsMaterial(const Path& saveTo, const std::string& filename = "NewPhysicsMaterial");
		static Path CreateSoundGroup(const Path& saveTo, const std::string& filename = "NewSoundGroup");

		static AssetType GetAssetTypeByExtension(const Path& filepath);

	private:
		// Internal functions
		static bool ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
	};
}
