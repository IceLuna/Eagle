#include "egpch.h"
#include "AssetUsagePanel.h"
#include "ContentBrowserPanel.h"
#include "../EditorLayer.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Core/AsyncTask.h"
#include "Eagle/Core/Project.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <ctime>
#include <numeric>

namespace Eagle
{
	namespace
	{
		constexpr ImVec4 s_WarningColor = ImVec4(1.f, 0.8f, 0.2f, 1.f);

		std::string FormatSize(size_t bytes)
		{
			const char* units[] = { "B", "KB", "MB", "GB", "TB" };
			double value = double(bytes);
			size_t unit = 0;
			while (value >= 1024.0 && unit + 1 < std::size(units))
			{
				value /= 1024.0;
				++unit;
			}

			char buffer[32];
			snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.1f %s", value, units[unit]);
			return buffer;
		}

		std::string GetTimestamp()
		{
			const std::time_t now = std::time(nullptr);
			std::tm localTime{};
#ifdef EG_PLATFORM_WINDOWS
			localtime_s(&localTime, &now);
#else
			localtime_r(&now, &localTime);
#endif
			char buffer[32];
			std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &localTime);
			return buffer;
		}

		std::string ToLower(std::string str)
		{
			for (auto& c : str)
				c = std::tolower(c);
			return str;
		}

		const char* GetModeDescription(AssetUsageMode mode)
		{
			switch (mode)
			{
				case AssetUsageMode::GameBuild:
					return "Assets that the game doesn't use. They aren't included in game builds";
				case AssetUsageMode::Project:
					return "Assets that aren't used by any scene";
				default:
					EG_CORE_ASSERT(false);
					return "<Invalid>";
			}
		}

		// A read-only table of assets. Used by the game build popup and by the tabs of the window
		void DrawAssetsTable(const char* id, const std::vector<const AssetUsageData*>& datas, const char* sourceColumn, ImVec2 size,
			const std::function<void(const AssetUsageData&)>& drawActions = {})
		{
			const int columnsCount = 4 + (sourceColumn ? 1 : 0) + (drawActions ? 1 : 0);
			constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
			if (!ImGui::BeginTable(id, columnsCount, flags, size))
				return;

			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 2.f);
			if (sourceColumn)
				ImGui::TableSetupColumn(sourceColumn, ImGuiTableColumnFlags_WidthStretch, 1.5f);
			if (drawActions)
				ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin(int(datas.size()));
			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
				{
					const AssetUsageData& data = *datas[row];
					ImGui::PushID(&data);
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(Utils::AsString(data.Filepath.stem()).c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(Utils::GetEnumName(data.Type));
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(FormatSize(data.FileSize).c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(Utils::AsString(data.Filepath).c_str());
					if (sourceColumn)
					{
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(Utils::AsString(data.Source).c_str());
					}
					if (drawActions)
					{
						ImGui::TableNextColumn();
						drawActions(data);
					}

					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
	}

	void AssetUsagePanel::SetOpened(bool bOpened)
	{
		m_bFocus = bOpened && !m_bOpened;
		m_bOpened = bOpened;
	}

	void AssetUsagePanel::OnImGuiRender()
	{
		if (m_bOpened)
			DrawWindow();

		DrawBuildPopup();
	}

	void AssetUsagePanel::DrawWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(1000.f, 600.f), ImGuiCond_FirstUseEver);
		if (m_bFocus)
		{
			ImGui::SetNextWindowFocus();
			m_bFocus = false;
		}

		if (!ImGui::Begin(GetWindowName(), &m_bOpened))
		{
			ImGui::End();
			return;
		}

		// Mode & scan button
		{
			ImGui::TextUnformatted("Find assets that:");
			ImGui::SameLine();
			if (ImGui::RadioButton("Aren't used by any scene", m_Mode == AssetUsageMode::Project))
				m_Mode = AssetUsageMode::Project;
			ImGui::SameLine();
			UI::HelpMarker("Every scene is considered used. An asset is used if a scene references it, or if a used asset references it, and so on.\n"
				"Use it to find the assets that can be removed from the project");
			ImGui::SameLine();
			if (ImGui::RadioButton("Aren't used by the game", m_Mode == AssetUsageMode::GameBuild))
				m_Mode = AssetUsageMode::GameBuild;
			ImGui::SameLine();
			UI::HelpMarker("Only the game startup scene is considered used. An asset is used if the startup scene references it, or if a used asset references it, and so on.\n"
				"These are the assets that are left out of game builds");

			ImGui::SameLine(0.f, 20.f);
			const bool bCanScan = !m_bScanning;
			if (!bCanScan)
				UI::PushItemDisabled();
			if (ImGui::Button(m_bHasReport ? "Scan again" : "Scan"))
				StartScan(m_Mode);
			if (!bCanScan)
				UI::PopItemDisabled();
			if (m_bScanning)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("Scanning...");
			}
		}

		ImGui::Separator();

		if (!m_bHasReport)
		{
			ImGui::TextWrapped("Press \"Scan\" to find the unused assets");
			ImGui::End();
			return;
		}

		ImGui::Text("%s. Found %zu (%s) in %.2fs. Used: %zu (%s)", GetModeDescription(m_Report.Mode),
			m_Report.Unused.size(), FormatSize(m_Report.UnusedSize).c_str(), m_Report.SecondsTook, m_Report.Used.size(), FormatSize(m_Report.UsedSize).c_str());
		if (m_DirtyAssetsCount > 0)
			ImGui::TextColored(s_WarningColor, "%zu asset(s) had unsaved changes. Assets are scanned as they're saved, so save them and scan again", m_DirtyAssetsCount);

		auto countByReason = [this](AssetUsageReason reason)
		{
			return size_t(std::count_if(m_Report.Used.begin(), m_Report.Used.end(), [reason](const AssetUsageData& data) { return data.Reason == reason; }));
		};

		if (ImGui::BeginTabBar("##AssetUsageTabs"))
		{
			const std::string unusedLabel = "Unused (" + std::to_string(m_Report.Unused.size()) + ")###Unused";
			if (ImGui::BeginTabItem(unusedLabel.c_str()))
			{
				DrawUnusedTab();
				ImGui::EndTabItem();
			}

			const std::string scriptsLabel = "Used by scripts (" + std::to_string(countByReason(AssetUsageReason::ScriptPath)) + ")###Scripts";
			if (ImGui::BeginTabItem(scriptsLabel.c_str()))
			{
				ImGui::TextWrapped("These assets are used because their paths are written in C# scripts. It's recommended to use public fields instead of manually writing asset paths");
				DrawUsedTab(AssetUsageReason::ScriptPath);
				ImGui::EndTabItem();
			}

			const std::string alwaysIncludedLabel = "Always included (" + std::to_string(countByReason(AssetUsageReason::AlwaysInclude)) + ")###AlwaysIncluded";
			if (ImGui::BeginTabItem(alwaysIncludedLabel.c_str()))
			{
				ImGui::TextWrapped("These assets are marked as \"Always build\". The assets they reference are used as well. "
					"The option can be changed in the asset's editor (double-click an asset to open it)");
				DrawUsedTab(AssetUsageReason::AlwaysInclude);
				ImGui::EndTabItem();
			}

			if (!m_Report.Warnings.empty())
			{
				const std::string warningsLabel = "Warnings (" + std::to_string(m_Report.Warnings.size()) + ")###Warnings";
				if (ImGui::BeginTabItem(warningsLabel.c_str()))
				{
					DrawWarningsTab();
					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}

		DrawRemovePopup();

		ImGui::End();
	}

	bool AssetUsagePanel::PassesFilter(const AssetUsageData& data) const
	{
		if (m_Filter.empty())
			return true;

		return ToLower(Utils::AsString(data.Filepath)).find(ToLower(m_Filter)) != std::string::npos
			|| ToLower(Utils::GetEnumName(data.Type)).find(ToLower(m_Filter)) != std::string::npos;
	}

	void AssetUsagePanel::SortUnused()
	{
		m_bSortRequested = false;

		const auto& unused = m_Report.Unused;
		m_UnusedOrder.resize(unused.size());
		std::iota(m_UnusedOrder.begin(), m_UnusedOrder.end(), size_t(0));

		const int column = m_SortColumn;
		const bool bAscending = m_bSortAscending;
		std::stable_sort(m_UnusedOrder.begin(), m_UnusedOrder.end(), [&unused, column, bAscending](size_t aIndex, size_t bIndex)
		{
			const AssetUsageData& a = unused[bAscending ? aIndex : bIndex];
			const AssetUsageData& b = unused[bAscending ? bIndex : aIndex];
			switch (column)
			{
				case 1:
					return Utils::AsString(a.Filepath.stem()) < Utils::AsString(b.Filepath.stem());
				case 2:
					return std::string_view(Utils::GetEnumName(a.Type)) < std::string_view(Utils::GetEnumName(b.Type));
				case 3:
					return a.FileSize < b.FileSize;
				default:
					return a.Filepath < b.Filepath;
			}
		});
	}

	void AssetUsagePanel::DrawUnusedTab()
	{
		auto& unused = m_Report.Unused;

		size_t selectedCount = 0;
		size_t selectedSize = 0;
		for (size_t i = 0; i < unused.size(); ++i)
		{
			if (m_Selected[i])
			{
				++selectedCount;
				selectedSize += unused[i].FileSize;
			}
		}

		// Toolbar
		{
			ImGui::SetNextItemWidth(260.f);
			UI::InputTextWithHint("##Filter", m_Filter, "Filter by path or type...");

			ImGui::SameLine();
			if (ImGui::Button("Select all"))
			{
				for (size_t i = 0; i < unused.size(); ++i)
					if (PassesFilter(unused[i]))
						m_Selected[i] = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Select none"))
				m_Selected.assign(unused.size(), false);

			const bool bEditing = EditorLayer::Get() && EditorLayer::Get()->GetEditorState() == EditorState::Edit;
			const bool bCanAct = selectedCount > 0 && !m_bScanning;

			ImGui::SameLine();
			const bool bCanRemove = bCanAct && bEditing;
			if (!bCanRemove)
				UI::PushItemDisabled();
			if (ImGui::Button("Remove selected..."))
				m_bOpenRemovePopup = true;
			if (!bCanRemove)
				UI::PopItemDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(bEditing ? "Moves the selected assets out of the project" : "Stop the simulation to remove assets");

			ImGui::SameLine();
			ImGui::Text("Selected: %zu (%s)", selectedCount, FormatSize(selectedSize).c_str());
		}

		constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
		if (!ImGui::BeginTable("##UnusedAssets", 5, flags, ImVec2(0.f, ImGui::GetContentRegionAvail().y)))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("##Selected", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize, ImGui::GetFrameHeight(), 0);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.f, 1);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0.f, 2);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.f, 3);
		ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 2.f, 4);
		ImGui::TableHeadersRow();

		if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsDirty)
		{
			if (sortSpecs->SpecsCount > 0)
			{
				m_SortColumn = int(sortSpecs->Specs[0].ColumnUserID);
				m_bSortAscending = sortSpecs->Specs[0].SortDirection != ImGuiSortDirection_Descending;
			}
			sortSpecs->SpecsDirty = false;
			m_bSortRequested = true;
		}
		if (m_bSortRequested || m_UnusedOrder.size() != unused.size())
			SortUnused();

		std::vector<size_t> visible;
		visible.reserve(m_UnusedOrder.size());
		for (size_t index : m_UnusedOrder)
			if (PassesFilter(unused[index]))
				visible.push_back(index);

		ImGuiListClipper clipper;
		clipper.Begin(int(visible.size()));
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				const size_t index = visible[row];
				const AssetUsageData& data = unused[index];
				const std::string pathString = Utils::AsString(data.Filepath);

				ImGui::PushID(&data);
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				bool bSelected = m_Selected[index];
				if (ImGui::Checkbox("##SelectAsset", &bSelected))
					m_Selected[index] = bSelected;

				ImGui::TableNextColumn();
				ImGui::Selectable(Utils::AsString(data.Filepath.stem()).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s\nDouble-click to open its editor", pathString.c_str());
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						OpenAssetEditor(data);
				}
				if (ImGui::BeginPopupContextItem("##AssetMenu"))
				{
					if (ImGui::MenuItem("Open the asset editor"))
						OpenAssetEditor(data);
					if (ImGui::MenuItem("Show in Content Browser"))
						ContentBrowserPanel::Get().ShowAsset(data.Filepath);
					ImGui::EndPopup();
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Utils::GetEnumName(data.Type));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FormatSize(data.FileSize).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pathString.c_str());

				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}

	void AssetUsagePanel::DrawUsedTab(AssetUsageReason reason)
	{
		std::vector<const AssetUsageData*> entries;
		for (const auto& data : m_Report.Used)
			if (data.Reason == reason)
				entries.push_back(&data);

		const ImVec2 size = ImVec2(0.f, ImGui::GetContentRegionAvail().y);
		if (reason == AssetUsageReason::ScriptPath)
		{
			DrawAssetsTable("##ScriptAssets", entries, "Script", size);
		}
		else
		{
			const AssetUsageData* toOpen = nullptr;
			DrawAssetsTable("##AlwaysIncludedAssets", entries, nullptr, size, [&toOpen](const AssetUsageData& data)
			{
				if (ImGui::SmallButton("Open"))
					toOpen = &data;
			});
			if (toOpen)
				OpenAssetEditor(*toOpen);
		}
	}

	void AssetUsagePanel::DrawWarningsTab()
	{
		if (ImGui::BeginChild("##Warnings"))
		{
			for (const auto& warning : m_Report.Warnings)
				ImGui::TextWrapped("%s", warning.c_str());
		}
		ImGui::EndChild();
	}

	void AssetUsagePanel::DrawRemovePopup()
	{
		constexpr const char* popupName = "Remove Unused Assets";
		if (m_bOpenRemovePopup)
		{
			ImGui::OpenPopup(popupName);
			m_bOpenRemovePopup = false;
		}

		if (!ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			return;

		size_t selectedCount = 0;
		size_t selectedSize = 0;
		for (size_t i = 0; i < m_Report.Unused.size(); ++i)
		{
			if (m_Selected[i])
			{
				++selectedCount;
				selectedSize += m_Report.Unused[i].FileSize;
			}
		}

		ImGui::PushTextWrapPos(600.f);
		ImGui::Text("%zu asset(s) (%s) will be moved out of the project to:", selectedCount, FormatSize(selectedSize).c_str());
		ImGui::TextDisabled("%s", Utils::AsString(Project::GetSavedPath() / "RemovedAssets").c_str());
		ImGui::TextWrapped("To restore an asset, move its file back into the project folder (keeping its path) and reopen the project.");
		if (m_Report.Mode == AssetUsageMode::GameBuild)
			ImGui::TextColored(s_WarningColor, "These assets aren't used by the game, but scenes that the game doesn't open might still use them!");
		if (m_DirtyAssetsCount > 0)
			ImGui::TextColored(s_WarningColor, "Some assets had unsaved changes when they were scanned. Unsaved references weren't taken into account!");
		ImGui::PopTextWrapPos();

		ImGui::Separator();
		if (ImGui::Button("Remove", ImVec2(120.f, 0.f)))
		{
			RemoveSelectedAssets();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	void AssetUsagePanel::DrawBuildPopup()
	{
		constexpr const char* popupName = "Unused Assets##GameBuild";
		if (m_bOpenBuildPopup && !AsyncTaskManager::IsModalTaskActive())
		{
			ImGui::OpenPopup(popupName);
			m_bOpenBuildPopup = false;
		}

		if (!ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			return;

		const auto& unused = m_BuildReport.Unused;
		constexpr float width = 900.f;

		ImGui::PushTextWrapPos(width);
		ImGui::Text("%zu asset(s) (%s) aren't used by the game, so they won't be included in the build:", unused.size(), FormatSize(m_BuildReport.UnusedSize).c_str());

		std::vector<const AssetUsageData*> datas;
		datas.reserve(unused.size());
		for (const auto& data : unused)
			datas.push_back(&data);

		const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().CellPadding.y * 2.f;
		const float tableHeight = rowHeight * float(std::min<size_t>(datas.size(), 15) + 1) + ImGui::GetStyle().ScrollbarSize;
		DrawAssetsTable("##ExcludedAssets", datas, nullptr, ImVec2(width, tableHeight));

		ImGui::TextWrapped("An asset is used by the game if the startup scene references it, or if a used asset references it, and so on. "
			"Assets whose paths are written in C# scripts, and assets marked as \"Always build\" are always used.");
		ImGui::TextColored(s_WarningColor, "If the game loads some of these assets in another way (for example, by a path that's built at runtime), "
			"enable \"Always build\" in their asset editors (for scenes, in the Scene Settings), save them, and build again.");
		if (!m_BuildReport.Warnings.empty())
			ImGui::TextColored(s_WarningColor, "There were %zu warning(s) while checking the assets. See the log.", m_BuildReport.Warnings.size());
		ImGui::PopTextWrapPos();

		ImGui::Separator();
		if (ImGui::Button("Build without them"))
		{
			std::vector<GUID> excluded;
			excluded.reserve(unused.size());
			for (const auto& data : unused)
				excluded.push_back(data.ID);

			Path buildFolder = FileDialog::OpenFolder();
			if (!buildFolder.empty())
			{
				Project::Build(buildFolder, excluded);
				m_BuildReport = {};
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Include all assets"))
		{
			Path buildFolder = FileDialog::OpenFolder();
			if (!buildFolder.empty())
			{
				Project::Build(buildFolder);
				m_BuildReport = {};
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_BuildReport = {};
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	void AssetUsagePanel::StartScan(AssetUsageMode mode)
	{
		if (m_bScanning)
			return;

		struct Job
		{
			AssetUsage::Request Request;
			AssetUsageReport Report;
		};
		auto job = MakeRef<Job>();
		job->Request = AssetUsage::CreateRequest(mode);

		m_bScanning = true;
		m_Mode = mode;
		const size_t dirtyAssetsCount = AssetManager::GetDirtyAssets().size();

		AsyncTaskDesc desc;
		desc.Name = "Finding unused assets";
		desc.bCancelable = true;
		desc.bModal = false;

		std::weak_ptr<bool> alive = m_Alive;
		AsyncTaskManager::Submit(desc,
			[job](AsyncTaskContext&)
			{
				return AssetUsage::Analyze(job->Request, &job->Report) ? AsyncTaskResult::Succeeded : AsyncTaskResult::Cancelled;
			},
			[this, alive, job, dirtyAssetsCount](AsyncTaskResult result)
			{
				if (alive.expired())
					return;

				m_bScanning = false;
				if (result == AsyncTaskResult::Failed)
					Application::Get().GetImGuiLayer()->AddMessage("Failed to find unused assets. See the log for more details");
				if (result != AsyncTaskResult::Succeeded)
					return;

				for (const auto& warning : job->Report.Warnings)
					EG_CORE_WARN("[Unused Assets] {}", warning);

				m_Report = std::move(job->Report);
				m_Selected.assign(m_Report.Unused.size(), false);
				m_UnusedOrder.clear();
				m_bSortRequested = true;
				m_bHasReport = true;
				m_DirtyAssetsCount = dirtyAssetsCount;
			});
	}

	void AssetUsagePanel::OpenAssetEditor(const AssetUsageData& data)
	{
		Ref<Asset> asset;
		if (!AssetManager::Get(data.ID, &asset))
		{
			Application::Get().GetImGuiLayer()->AddMessage("The asset doesn't exist anymore: " + Utils::AsString(data.Filepath));
			return;
		}

		if (asset->GetAssetType() == AssetType::Scene)
		{
			ContentBrowserPanel::Get().ShowAsset(data.Filepath);
			Application::Get().GetImGuiLayer()->AddMessage("Scenes don't have a regular editor. "
				"Open the scene, and use \"Always build\" in the Scene Settings");
			return;
		}

		ContentBrowserPanel::Get().OpenAssetEditor(asset);
	}

	void AssetUsagePanel::RemoveSelectedAssets()
	{
		const Path destinationFolder = Project::GetSavedPath() / "RemovedAssets" / GetTimestamp();
		const Ref<AssetScene> openedScene = EditorLayer::Get() ? EditorLayer::Get()->GetOpenedSceneAsset() : nullptr;
		const Ref<AssetScene> startupScene = Project::GetProjectInfo().GameStartupScene;

		std::vector<AssetUsageData> remaining;
		std::vector<bool> remainingSelected;
		size_t movedCount = 0;
		std::vector<std::string> notRemoved;

		auto& unused = m_Report.Unused;
		for (size_t i = 0; i < unused.size(); ++i)
		{
			AssetUsageData& data = unused[i];
			if (!m_Selected[i])
			{
				remaining.push_back(std::move(data));
				remainingSelected.push_back(false);
				continue;
			}

			// The project might have changed since the scan
			Ref<Asset> asset;
			if (!AssetManager::Get(data.Filepath, &asset) || asset->GetGUID() != data.ID)
			{
				if (std::filesystem::exists(data.Filepath))
				{
					notRemoved.push_back(Utils::AsString(data.Filepath) + " (it was changed after the scan)");
					remaining.push_back(std::move(data));
					remainingSelected.push_back(true);
				}
				continue; // It doesn't exist anymore
			}

			if (asset == openedScene || asset == startupScene)
			{
				notRemoved.push_back(Utils::AsString(data.Filepath) + (asset == openedScene ? " (it's the opened scene)" : " (it's the game startup scene)"));
				remaining.push_back(std::move(data));
				remainingSelected.push_back(true);
				continue;
			}

			ContentBrowserPanel::Get().CloseAssetEditor(asset);
			if (AssetManager::MoveOutOfProject(asset, destinationFolder))
			{
				++movedCount;
				m_Report.UnusedSize -= std::min(m_Report.UnusedSize, data.FileSize);
				continue;
			}

			notRemoved.push_back(Utils::AsString(data.Filepath) + " (couldn't move the file, see the log)");
			remaining.push_back(std::move(data));
			remainingSelected.push_back(true);
		}

		unused = std::move(remaining);
		m_Selected = std::move(remainingSelected);
		m_UnusedOrder.clear();
		m_bSortRequested = true;
		ContentBrowserPanel::RequestRefresh();

		std::string message = "Moved " + std::to_string(movedCount) + " asset(s) out of the project to:\n" + Utils::AsString(destinationFolder);
		if (!notRemoved.empty())
		{
			message += "\n\nThese assets weren't removed:";
			constexpr size_t maxListed = 10;
			for (size_t i = 0; i < notRemoved.size() && i < maxListed; ++i)
				message += "\n" + notRemoved[i];
			if (notRemoved.size() > maxListed)
				message += "\n...and " + std::to_string(notRemoved.size() - maxListed) + " more";
		}
		EG_CORE_INFO("[Unused Assets] {}", message);
		Application::Get().GetImGuiLayer()->AddMessage(message);
	}

	void AssetUsagePanel::BuildGame()
	{
		if (m_bPreparingBuild || m_bOpenBuildPopup)
			return;

		struct Job
		{
			AssetUsage::Request Request;
			AssetUsageReport Report;
		};
		auto job = MakeRef<Job>();
		job->Request = AssetUsage::CreateRequest(AssetUsageMode::GameBuild);

		m_bPreparingBuild = true;

		AsyncTaskDesc desc;
		desc.Name = "Preparing the game build";
		desc.bCancelable = true;
		desc.bModal = true;

		std::weak_ptr<bool> alive = m_Alive;
		AsyncTaskManager::Submit(desc,
			[job](AsyncTaskContext&)
			{
				return AssetUsage::Analyze(job->Request, &job->Report) ? AsyncTaskResult::Succeeded : AsyncTaskResult::Cancelled;
			},
			[this, alive, job](AsyncTaskResult result)
			{
				if (alive.expired())
					return;

				m_bPreparingBuild = false;
				if (result == AsyncTaskResult::Failed)
				{
					Application::Get().GetImGuiLayer()->AddMessage("Failed to find the assets that the game uses. See the log for more details");
					return;
				}
				if (result != AsyncTaskResult::Succeeded)
					return;

				for (const auto& warning : job->Report.Warnings)
					EG_CORE_WARN("[Game build] {}", warning);

				if (job->Report.Unused.empty())
				{
					Path buildFolder = FileDialog::OpenFolder();
					if (buildFolder.empty())
					{
						return; // Canceled
					}

					Project::Build(buildFolder);
					return;
				}

				m_BuildReport = std::move(job->Report);
				m_bOpenBuildPopup = true;
			});
	}
}
