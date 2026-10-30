#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Core/SplashScreen.h"
#include "Eagle/Renderer/RendererUtils.h"

namespace Eagle
{
	class AssetScene;

	// Name, Mask
	using CollisionGroupInfo = std::pair<std::string, uint32_t>;

	// The splash screen a game build shows while it's starting
	struct GameSplashScreenSettings
	{
		Path Image; // Relative to the project folder. Empty - the default background
		Path Icon;  // Relative to the project folder. It's also the icon of the game's window & executable. Empty - no icon
		std::string Title; // Empty - the project's name
		std::string Subtitle;
		bool bShowTextOverImage = false;
		SplashScreenColors Colors;
	};

	struct ProjectInfo
	{
		Path BasePath; // Path to the folder that contains `.egproj`
		std::string Name;
		glm::uvec3 Version = glm::uvec3(1, 0, 0); // Major, minor, patch
		Ref<AssetScene> GameStartupScene;
		std::vector<CollisionGroupInfo> UserCollisionGroups;
		std::vector<CollisionGroupInfo> AllCollisionGroups;
		uint32_t AllCollisionGroupsMask = 0u; // It's a XOR of all collision groups. Can be used to check if as certain mask is valid within a project. (Used to reset a mask if it was removed)
		std::vector<GUID64> CollisionGroupGUIDs; // GUIDs of all collision groups. Used to identify if groups were changed
		UIScaleSettings UIScale;
		GameSplashScreenSettings SplashScreen;
	};

	class Project
	{
	public:
		// Doesn't open the project, just creates files
		// @path. Path to folder to create project in
		static bool Create(const ProjectInfo& info);

		// @filepath. Path to `.egproj`
		static bool Open(const Path& filepath);
		static bool Close();
		static void GenerateSolution(const ProjectInfo& info);
		// @excludedAssets. Assets that aren't packed into the build
		static void Build(const Path& outputFolder, const std::vector<GUID>& excludedAssets = {});
		static void OpenGameBuild(const Path& filepath); // Path to .egpack
		static void Save();
		static void Save(const ProjectInfo& info);
		static void OnProjectOpenProcessed(); // Called when application has finished resetting & initializing systems

		static void SetStartupScene(const Ref<AssetScene>& scene) { s_Info.GameStartupScene = scene; }
		static void SetVersion(const glm::uvec3& version) { s_Info.Version = version; }
		static void SetUIScaleSettings(const UIScaleSettings& settings) { s_Info.UIScale = settings; }
		static void SetSplashScreenSettings(const GameSplashScreenSettings& settings) { s_Info.SplashScreen = settings; }

		static const ProjectInfo& GetProjectInfo() { return s_Info; }

		static const uint32_t GetValidCollisionGroupsMask() { return s_Info.AllCollisionGroupsMask; }
		static const std::vector<CollisionGroupInfo>& GetAllCollisionGroups() { return s_Info.AllCollisionGroups; }
		static const std::vector<CollisionGroupInfo>& GetUserCollisionGroups() { return s_Info.UserCollisionGroups; }
		static void AddUserCollisionGroup(const CollisionGroupInfo& group, const GUID64& guid);
		static void AddUserCollisionGroup(const std::string& name);
		static void RemoveUserCollisionGroup(uint32_t mask);
		static void RenameUserCollisionGroup(uint32_t mask, const std::string& newName);
		static bool CanAddUserCollisionGroup();
		static GUID64 GetCollisionGroupGUIDByMask(uint32_t mask);

		static const Path& GetProjectPath() { return s_Info.BasePath; }
		static const Path GetProjectFilePath() { return s_Info.BasePath / (s_Info.Name + Project::GetExtension()); }
		static bool IsOpened() { return !s_Info.BasePath.empty(); }

		static Path GetBinariesPath() { return GetProjectPath() / "Binaries"; }
		static Path GetConfigPath() { return GetProjectPath() / "Config"; }
		static Path GetContentPath() { return GetProjectPath() / "Content"; }
		static Path GetCachePath() { return GetProjectPath() / "Cache"; }
		static Path GetRendererCachePath() { return GetCachePath() / "Renderer"; }
		static Path GetSavedPath() { return GetProjectPath() / "Saved"; }
		static Path GetSourcePath() { return GetProjectPath() / "Source"; }
		static Path GetPhysicsDebugInfoPath() { return Project::GetSavedPath() / "PhysXDebugInfo"; }

		static Path GetShaderPackRelativePath() { return "Data/ShaderPack.egspack"; }

		// Image formats the splash screen image & icon can have
		static const std::vector<std::string>& GetSplashScreenImageExtensions();

		// The game build copies the splash screen's image & icon into `Data`, and writes the rest of its settings into this file.
		// So that the game can show its splash screen before it loads anything
		static Path GetGameBuildSplashScreenRelativePath() { return "Data/Splash.yaml"; }

		// @baseFolder. The folder that the paths in `settings` are relative to
		static SplashScreenDesc MakeGameSplashScreenDesc(const GameSplashScreenSettings& settings, const std::string& projectName, const Path& baseFolder);

		static SplashScreenDesc GetGameSplashScreenDesc() { return MakeGameSplashScreenDesc(s_Info.SplashScreen, s_Info.Name, s_Info.BasePath); }

		static bool LoadGameBuildSplashScreen(SplashScreenDesc* outDesc);

		// @projectFile. Path to `.egproj`
		static std::string ReadProjectName(const Path& projectFile);

		static const char* GetExtension() { return ".egproj"; }

	private:
		static bool Load(const Path& filepath, ProjectInfo* outInfo);

	private:
		static ProjectInfo s_Info;
	};
}
