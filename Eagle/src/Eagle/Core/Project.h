#pragma once

#include <filesystem>

namespace Eagle
{
	class Project
	{
	public:
		static Path GetProjectPath() { return "..\\Sandbox"; } // TODO: Change `Sandbox` to project name

		static Path GetContentPath() { return GetProjectPath() / "Content"; }
		static Path GetCachePath() { return GetProjectPath() / "Cache"; }
		static Path GetRendererCachePath() { return GetCachePath() / "Renderer"; }
		static Path GetSavedPath() { return GetProjectPath() / "Saved"; }
	};
}
