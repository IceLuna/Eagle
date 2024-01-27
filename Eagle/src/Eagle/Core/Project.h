#pragma once

namespace Eagle
{
	class AssetScene;

	struct ProjectInfo
	{
		Path BasePath; // path to the folder that contains `.egproj`
		std::string Name;
		glm::uvec3 Version = glm::uvec3(1, 0, 0); // Major, minor, patch
		Ref<AssetScene> GameStartupScene;
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

		static void SetStartupScene(const Ref<AssetScene>& scene) { s_Info.GameStartupScene = scene; }
		static void SetVersion(const glm::uvec3& version) { s_Info.Version = version; }

		static const ProjectInfo& GetProjectInfo() { return s_Info; }

		static const Path& GetProjectPath() { return s_Info.BasePath; }

		static Path GetBinariesPath() { return GetProjectPath() / "Binaries"; }
		static Path GetConfigPath() { return GetProjectPath() / "Config"; }
		static Path GetContentPath() { return GetProjectPath() / "Content"; }
		static Path GetCachePath() { return GetProjectPath() / "Cache"; }
		static Path GetRendererCachePath() { return GetCachePath() / "Renderer"; }
		static Path GetSavedPath() { return GetProjectPath() / "Saved"; }

	private:
		static ProjectInfo s_Info;
	};
}
