#pragma once

#include <filesystem>

namespace Eagle
{
	struct ProjectInfo
	{
		Path BasePath; // path to the folder that contains `.egproj`
		std::string Name;
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

		static const ProjectInfo& GetProjectInfo() { return s_Info; }

		static Path GetProjectPath() { return s_Info.BasePath; }

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
