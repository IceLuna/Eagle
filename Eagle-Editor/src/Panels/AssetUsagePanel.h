#pragma once

#include "Eagle/Asset/AssetUsage.h"

namespace Eagle
{
	// Shows the assets that aren't used (by the game, or by any scene).
	// Also, before a game is built, finds the assets that the game doesn't use, shows them to the user, and leaves them out of the build.
	class AssetUsagePanel
	{
	public:
		AssetUsagePanel() = default;

		void OnImGuiRender();

		// Finds the assets that the game doesn't use and asks the user to confirm that they can be left out. Then builds the game
		void BuildGame();

		void SetOpened(bool bOpened);
		bool IsOpened() const { return m_bOpened; }

		static const char* GetWindowName() { return "Unused Assets"; }

	private:
		void DrawWindow();
		void DrawUnusedTab();
		void DrawUsedTab(AssetUsageReason reason);
		void DrawWarningsTab();
		void DrawRemovePopup();
		void DrawBuildPopup();

		void StartScan(AssetUsageMode mode);
		void OpenAssetEditor(const AssetUsageData& entry);
		void RemoveSelectedAssets();
		void SortUnused();
		bool PassesFilter(const AssetUsageData& entry) const;

	private:
		// Callbacks of background tasks check it, since the panel might be destroyed before they're executed
		Ref<bool> m_Alive = MakeRef<bool>(true);

		AssetUsageReport m_Report;
		std::vector<size_t> m_UnusedOrder; // Indices into `m_Report.Unused`, in the displayed order
		std::vector<bool> m_Selected; // Per `m_Report.Unused` entry
		std::string m_Filter;
		AssetUsageMode m_Mode = AssetUsageMode::Project;
		bool m_bOpened = false;
		bool m_bFocus = false;
		bool m_bHasReport = false;
		bool m_bScanning = false;
		bool m_bSortRequested = true;
		int m_SortColumn = 4;
		bool m_bSortAscending = true;
		size_t m_DirtyAssetsCount = 0;
		bool m_bOpenRemovePopup = false;

		// Game build
		AssetUsageReport m_BuildReport;
		bool m_bPreparingBuild = false;
		bool m_bOpenBuildPopup = false;
	};
}
