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
		TextureCompressor::Quality Compression = TextureCompressor::Quality::Medium;
		bool bNormalMap = false;
	};

	struct AssetImportTextureCubeSettings
	{
		AssetTextureCubeFormat ImportFormat = AssetTextureCubeFormat::Default;
		uint32_t LayerSize = 512;
		uint32_t PrefilterSize = 512;
	};

	struct AssetImportMeshSettings
	{
		bool bImportMaterials = true;
		bool bImportAnimations = true;
	};

	struct AssetImportAnimationSettings
	{
		Ref<AssetSkeletalMesh> Skeletal;
		RootMotionMode RootMotionType = RootMotionMode::Disabled;
	};

	struct AssetImportSettings
	{
		AssetImportTexture2DSettings Texture2DSettings;
		AssetImportTextureCubeSettings TextureCubeSettings;
		AssetImportMeshSettings MeshSettings;
		AssetImportAnimationSettings AnimationSettings;

		bool bOnlyImportAnimations = false;
	};

	// The purpose of this class is to take a path to a raw asset data (such as `.png`, `.fbx`, etc...)
	// and convert it into `.egasset` file format.
	class AssetImporter
	{
	public:
		// Returns true, if an eagle asset was successfully created
		// @pathToRaw - path to a raw asset (such as `png`, `.fbx`, etc...)
		// @saveTo - folder to save an imported asset to (must be somewhere within projects content folder)
		static bool Import(const Path& pathToRaw, const Path& saveTo, AssetType type, const AssetImportSettings& settings);

		// Can be used to import multiple assets at once
		// @pathsToRaw - paths to raw assets (such as `png`, `.fbx`, etc...)
		// @saveTo - folder to save an imported asset to (must be somewhere within projects content folder)
		static void Import(const std::vector<Path>& pathsToRaw, const Path& saveTo);

		// @buffer. Buffer of encoded image data (png/jpg etc)
		// @saveTo. Folder to save to
		// @filename. Desired filename. If the filename already exists, new one will be generated. Check it by calling `asset->GetPath()`
		// @settings. Import settings
		static Ref<AssetTexture2D> ImportTexture2DFromMemory(DataBuffer buffer, const Path& saveTo, const std::string& filename, const AssetImportTexture2DSettings& settings);

		// Since materials are not really imported, this function will just generate a material asset.
		// The final `filename` might be a bit different if the requested filename is already taken.
		// So the function returns the final path to the file it just created.
		// @saveTo - folder to save an imported asset to (must be somewhere within projects content folder)
		// @filename - filename without an extension
		static Path CreateMaterial(const Path& saveTo, const std::string& filename = "NewMaterial");
		static Path CreatePhysicsMaterial(const Path& saveTo, const std::string& filename = "NewPhysicsMaterial");
		static Path CreateSoundGroup(const Path& saveTo, const std::string& filename = "NewSoundGroup");
		static Path CreateEntity(const Path& saveTo, const std::string& filename = "NewEntity");
		static Path CreateScene(const Path& saveTo, const std::string& filename = "NewScene");
		static Path CreateAnimationGraph(const Path& saveTo, const Ref<AssetSkeletalMesh>& skeletal, const std::string& filename = "NewAnimationGraph");
		static Path CreateParticleSystem(const Path& saveTo, const std::string& filename = "NewParticleSystem");
		static Path CreateAnimationBlendSpace(const Path& saveTo, const Ref<AssetSkeletalMesh>& skeletal, const std::string& filename = "NewAnimationBlendSpace");
		static Path CreateBehaviorGraph(const Path& saveTo, const std::string& filename = "NewBehaviorGraph");

		static AssetType GetAssetTypeByExtension(const Path& filepath);

	private:
		// Internal functions
		static bool ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportAnimation(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportAnimationSettings& settings);
	};
}
