#pragma once

#include <filesystem>

namespace Eagle
{
	class Project
	{
	public:
		static std::filesystem::path GetProjectPath() { return "../Sandbox"; }

		static std::filesystem::path GetContentPath() { return GetProjectPath() / "Content"; }
		static std::filesystem::path GetCachePath() { return GetProjectPath() / "Cache"; }
		static std::filesystem::path GetSavedPath() { return GetProjectPath() / "Saved"; }
	};
}
