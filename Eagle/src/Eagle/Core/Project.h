#pragma once

#include "Eagle/Core/GUID.h"
#include "Eagle/Core/SplashScreen.h"
#include "Eagle/Renderer/RendererUtils.h"
#include "Eagle/AI/NavigationMesh.h"

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

		// There's always at least one agent type
		std::vector<AINavigation::AgentType> NavAgentTypes;
		uint32_t DefaultNavAgentTypeID = AINavigation::InvalidAgentTypeID; // Used by queries and entities that don't pick an agent type
		uint32_t NextNavAgentTypeID = 1u;

		// The game (also when played in the editor) doesn't update while its window is minimized.
		// Otherwise, the engine keeps updating its systems without rendering
		bool bPauseWhenMinimized = false;
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
		// @bScriptDebugging. Whether script debuggers can attach to the game
		static void Build(const Path& outputFolder, const std::vector<GUID>& excludedAssets = {}, bool bScriptDebugging = false);
		static void OpenGameBuild(const Path& filepath); // Path to .egpack
		static void Save();
		static void Save(const ProjectInfo& info);
		static void OnProjectOpenProcessed(); // Called when application has finished resetting & initializing systems

		static void SetStartupScene(const Ref<AssetScene>& scene) { s_Info.GameStartupScene = scene; }
		static void SetVersion(const glm::uvec3& version) { s_Info.Version = version; }
		static void SetPauseWhenMinimized(bool bPause) { s_Info.bPauseWhenMinimized = bPause; }
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

		static const std::vector<AINavigation::AgentType>& GetNavAgentTypes() { return s_Info.NavAgentTypes; }
		static uint32_t GetDefaultNavAgentTypeID() { return s_Info.DefaultNavAgentTypeID; }
		static const AINavigation::AgentType& GetDefaultNavAgentType(); // Note: returned reference is invalidated when agent types are added or removed
		static void SetDefaultNavAgentType(uint32_t id);
		static bool CanAddNavAgentType() { return s_Info.NavAgentTypes.size() < AINavigation::MaxAgentTypes; }
		static bool CanRemoveNavAgentType() { return s_Info.NavAgentTypes.size() > 1u; }

		// Null if there's no agent type with this ID/name.
		// Note: returned pointer is invalidated when agent types are added or removed
		static const AINavigation::AgentType* FindNavAgentType(uint32_t id);
		static const AINavigation::AgentType* FindNavAgentTypeByName(std::string_view name);

		// Returns the ID of the new type, or `InvalidAgentTypeID` if failed. Its name is made unique
		static uint32_t AddNavAgentType(const std::string& name);

		// The last agent type can't be removed. If the default type is removed, the first one becomes the default
		static bool RemoveNavAgentType(uint32_t id);

		// A name must be unique and not empty. Returns false and keeps the old name otherwise
		static bool RenameNavAgentType(uint32_t id, const std::string& name);

		// Sets everything but the ID and the name of the type with `type.ID` (see `RenameNavAgentType()`). Values are clamped
		static void SetNavAgentType(const AINavigation::AgentType& type);

		static const Path& GetProjectPath() { return s_Info.BasePath; }
		static const Path GetProjectFilePath() { return s_Info.BasePath / (s_Info.Name + Project::GetExtension()); }
		static bool IsOpened() { return !s_Info.BasePath.empty(); }

		static Path GetBinariesPath() { return GetProjectPath() / "Binaries"; }
		static Path GetConfigPath() { return GetProjectPath() / "Config"; }
		static Path GetContentPath() { return GetProjectPath() / "Content"; }
		static Path GetCachePath() { return GetProjectPath() / "Cache"; }
		static Path GetRendererCachePath() { return GetCachePath() / "Renderer"; }
		static Path GetScriptsDebuggerCachePath() { return GetCachePath() / "Scripts"; }
		static Path GetSavedPath() { return GetProjectPath() / "Saved"; }
		static Path GetSourcePath() { return GetProjectPath() / "Source"; }
		static Path GetPhysicsDebugInfoPath() { return Project::GetSavedPath() / "PhysXDebugInfo"; }

		static Path GetShaderPackRelativePath() { return "Data/ShaderPack.egspack"; }

		// Image formats the splash screen image & icon can have
		static const std::vector<std::string>& GetSplashScreenImageExtensions();

		// The game build copies the splash screen's image & icon into `Data`, and writes the rest of its settings into this file.
		// So that the game can show its splash screen before it loads anything
		static Path GetGameBuildSplashScreenRelativePath() { return "Data/Splash.yaml"; }

		// A game built with script debugging has this file
		static Path GetGameDebuggingSettingsRelativePath() { return "Data/Debugging.yaml"; }

		// Games only. Whether the game was built with script debugging. False if the file is missing or can't be read
		static bool IsGameBuildScriptDebuggingEnabled();

		// @baseFolder. The folder that the paths in `settings` are relative to
		static SplashScreenDesc MakeGameSplashScreenDesc(const GameSplashScreenSettings& settings, const std::string& projectName, const Path& baseFolder);

		static SplashScreenDesc GetGameSplashScreenDesc() { return MakeGameSplashScreenDesc(s_Info.SplashScreen, s_Info.Name, s_Info.BasePath); }

		static bool LoadGameBuildSplashScreen(SplashScreenDesc* outDesc);

		// @projectFile. Path to `.egproj`
		static std::string ReadProjectName(const Path& projectFile);

		static const char* GetExtension() { return ".egproj"; }

	private:
		static bool Load(const Path& filepath, ProjectInfo* outInfo);
		static bool IsNavAgentTypeNameAvailable(std::string_view name, uint32_t exceptID = AINavigation::InvalidAgentTypeID);

	private:
		static ProjectInfo s_Info;
	};
}
