#pragma once

#include "Eagle/Asset/Asset.h"
#include "Eagle/Core/GUID.h"

namespace Eagle
{
	// Determines which assets count as used. An asset is used if it can be reached from a "root" by following references
	enum class AssetUsageMode
	{
		// Root: the game startup scene. Unused assets aren't needed by the game, so they're excluded from game builds
		GameBuild,

		// Root: every scene. Unused assets aren't used by anything in the project, so they're candidates for removal
		Project,
	};

	enum class AssetUsageReason : uint8_t
	{
		Unused,
		StartupScene,
		Scene,
		AlwaysInclude,
		ScriptPath, // Its path is mentioned in a C# source file (for `Asset.Get("Content/...")`)
		Referenced,
	};

	struct AssetUsageData
	{
		Path Filepath;
		GUID ID;
		AssetType Type = AssetType::None;
		size_t FileSize = 0;
		AssetUsageReason Reason = AssetUsageReason::Unused;
		Path Source; // For `Referenced`, the path of the asset that references it. For `ScriptPath`, the C# file
	};

	struct AssetUsageReport
	{
		AssetUsageMode Mode = AssetUsageMode::GameBuild;
		std::vector<AssetUsageData> Used;   // Sorted by path
		std::vector<AssetUsageData> Unused; // Sorted by path
		size_t UsedSize = 0;   // Total size of the files
		size_t UnusedSize = 0; // Total size of the files
		std::vector<std::string> Warnings;
		float SecondsTook = 0.f;
	};

	// Finds out which assets of the project are used.
	// Note: an asset that's only loaded by a path that's built at runtime (e.g. "Content/Levels/" + name + ".egasset") can't be detected.
	// Such assets should be marked as "Always include in game builds".
	// The logic here reads the saved asset files, so unsaved changes aren't taken into account
	class AssetUsage
	{
	public:
		struct Request
		{
			struct AssetInfo
			{
				Path Filepath;
				GUID ID;
				AssetType Type = AssetType::None;
			};

			AssetUsageMode Mode = AssetUsageMode::GameBuild;
			std::vector<AssetInfo> Assets;
			std::vector<GUID> AlwaysIncluded;
			GUID StartupScene = GUID(0, 0);
			Path ScriptsFolder;
		};

		// Main thread only. Takes a snapshot of the project's assets
		static Request CreateRequest(AssetUsageMode mode);

		// If it's called from an async task, it reports progress, and stops if a cancellation is requested.
		// Returns true on success.
		static bool Analyze(const Request& request, AssetUsageReport* outReport);
	};
}
