#pragma once

#include <filesystem>

namespace Eagle
{
	class Project
	{
	public:
		static Path GetProjectPath() { return "..\\Sandbox"; }

		static Path GetContentPath() { return GetProjectPath() / "Content"; }
		static Path GetCachePath() { return GetProjectPath() / "Cache"; }
		static Path GetRendererCachePath() { return GetProjectPath() / "Cache/Renderer"; }
		static Path GetSavedPath() { return GetProjectPath() / "Saved"; }
	};
}
