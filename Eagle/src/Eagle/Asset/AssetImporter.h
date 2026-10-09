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
		bool bCompress = true;
	};

	struct AssetImportMeshSettings
	{
		bool bImportMaterials = true;
		bool bImportAnimations = true;

		// If true, all meshes found in the source file are merged into a single Static/Skeletal Mesh asset.
		// If false, every mesh in the file is imported as its own separate asset
		bool bCombineMeshes = true;

		// Meshes normally keep the position they had
		// within the source file, which is usually not (0, 0, 0) since they're separated from the rest of the meshes.
		// If true, each mesh's location gets reset to zero on import. Rotation & scale are kept. Only has an effect when `Combine Meshes` is disabled.
		bool bResetLocation = false;

		// If true, a scene asset will be create that contains imported mesh assets
		bool bCreateScene = false;
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

	// One asset file produced by a mesh import, along with every place it needs to be instantiated to
	struct MeshImportResult
	{
		Path OutputFilename;
		std::vector<Utils::MeshInstanceImportData> Instances;
	};

	struct AssetImportRequest
	{
		Path PathToRaw;
		AssetType Type = AssetType::None; // If `None`, it's detected by the file extension
		AssetImportSettings Settings;
	};

	struct AssetImportAsyncResult
	{
		uint32_t Succeeded = 0;
		uint32_t Failed = 0;
		uint32_t Skipped = 0; // Files that weren't imported because the import was cancelled
		bool bCancelled = false;
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

		// Imports assets on a background thread, so the called doesn't freeze.
		// A progress bar with a `Cancel` button is shown while it's running.
		// The assets of each file are registered as soon as that file is imported.
		// Cancelling stops at the next asset. The file that was being imported is rolled back (the files it wrote are deleted), files that were already imported are kept.
		// @saveTo - folder to save imported assets to (must be somewhere within projects content folder)
		// @onFinished - main thread. Called once everything is done. Optional
		// @onFileImported - main thread. Called each time a file's assets were registered. Optional
		static void ImportAsync(std::vector<AssetImportRequest> requests, const Path& saveTo,
			std::function<void(const AssetImportAsyncResult&)> onFinished = {}, std::function<void()> onFileImported = {});

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
		static Path CreateSceneSequence(const Path& saveTo, const std::string& filename = "NewSceneSequence");

		static AssetType GetAssetTypeByExtension(const Path& filepath);

	private:
		// Internal functions
		static bool ImportTexture2D(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportTextureCube(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		// Returns elements per asset file written.
		// More than one element is returned when `settings.MeshSettings.bCombineMeshes == false` and the source file contains multiple meshes;
		// a single element can itself carry multiple instances when its source mesh was instanced
		static std::vector<MeshImportResult> ImportStaticMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings);
		static std::vector<MeshImportResult> ImportSkeletalMesh(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportAudio(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);
		static bool ImportFont(const Path& pathToRaw, const Path& outputFilename, const AssetImportSettings& settings);

		// Can import multiple animations. Returns filepaths for all of them. Empty on failure
		static std::vector<Path> ImportAnimations(const Path& pathToRaw, const Path& saveTo, const Path& outputFilename, const AssetImportAnimationSettings& settings);
	};
}
