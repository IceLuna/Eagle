#pragma once

#include "Eagle/Core/GUID.h"

namespace Eagle
{
	class AssetScene;

	// Name, Mask
	using CollisionGroupInfo = std::pair<std::string, uint32_t>;

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
		static void Build(const Path& outputFolder);
		static void OpenGameBuild(const Path& filepath); // Path to .egpack
		static void Save();
		static void Save(const ProjectInfo& info);
		static void OnProjectOpenProcessed(); // Called when application has finished resetting & initializing systems

		static void SetStartupScene(const Ref<AssetScene>& scene) { s_Info.GameStartupScene = scene; }
		static void SetVersion(const glm::uvec3& version) { s_Info.Version = version; }

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
		static Path GetPhysicsDebugInfoPath() { return Project::GetSavedPath() / "PhysXDebugInfo"; }

		static const char* GetExtension() { return ".egproj"; }

	private:
		static bool Load(const Path& filepath, ProjectInfo* outInfo);

	private:
		static ProjectInfo s_Info;
	};
}
