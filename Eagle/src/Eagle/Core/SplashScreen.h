#pragma once

#include "Eagle/Core/Core.h"

#include <glm/glm.hpp>

namespace Eagle
{
	struct SplashScreenColors
	{
		glm::vec3 Title = glm::vec3(0.949f, 0.949f, 0.949f);
		glm::vec3 Subtitle = glm::vec3(0.651f, 0.667f, 0.69f);
		glm::vec3 Progress = glm::vec3(0.878f, 0.659f, 0.180f);

		bool operator==(const SplashScreenColors&) const = default;
	};

	struct SplashScreenDesc
	{
		std::string Title;
		std::string Subtitle;
		Path ImagePath; // Optional (png, jpg, bmp). Replaces the default background. The splash screen takes the image's aspect ratio
		Path IconPath;  // Optional. The taskbar icon. It's also drawn next to the title

		bool bShowTextOverImage = false; // Note: without a custom image, text is always drawn
		SplashScreenColors Colors;

		// Optional. A native window handle. The splash screen is centered on its monitor. If not set, it's centered on the primary monitor
		void* NearWindow = nullptr;
	};

	// It runs on its own thread with its own message loop, so it stays responsive and keeps updating while the main thread is blocked.
	// All functions are thread-safe. Status and progress can be reported from any thread.
	class SplashScreen
	{
	public:
		static void Show(const SplashScreenDesc& desc);
		static void Hide();
		static bool IsVisible();

		static void SetSubtitle(const std::string& subtitle);
		static void SetStatus(std::string_view status);

		// [0; 1]. A negative value means that the progress is unknown, the progress bar is animated instead
		static void SetProgress(float progress);
	};
}
