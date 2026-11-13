#include "egpch.h"
#include "ProjectSettingsTab.h"

#include "../EditorLayer.h"
#include "../EditorResources.h"

#include "Eagle/Core/Project.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	namespace
	{
		struct SectionInfo
		{
			ProjectSettingsTab::Section Value;
			const char* Name;
			const char* Description;
		};

		// In the order of the list
		constexpr SectionInfo s_Sections[] =
		{
			{ ProjectSettingsTab::Section::General, "General", "The scene the game starts with, the version of the game, and whether it runs while its window is minimized" },
			{ ProjectSettingsTab::Section::CollisionGroups, "Collision Groups",
				"Groups of physics objects. Colliders, character controllers and ragdolls belong to groups, and choose which groups they interact with.\n"
				"Deleting a group removes it from everything that uses it" },
			{ ProjectSettingsTab::Section::Navigation, "Navigation",
				"Agent types for AI navigation. A scene has a nav mesh per agent type, built for the agent's size. Crowd agents use the nav mesh that fits them, "
				"unless they pick a type. Other queries use the default type, unless a script picks another one.\n"
				"Deleting a type makes nav meshes that used it fall back to other types, and crowd agents and script fields fall back to `Auto`" },
			{ ProjectSettingsTab::Section::UIScale, "UI Scale", "How the 2D UI (for example, Text2D and Image2D components) is scaled to the resolution of the viewport" },
			{ ProjectSettingsTab::Section::SplashScreen, "Game Splash Screen",
				"Shown while the game is starting, until its first frames are rendered. It doesn't affect the editor, it has its own splash screen" },
		};

		// Value widgets don't stretch across the whole value column, so that they don't get very wide in a wide window
		constexpr float s_MaxValueWidth = 20.f;
		constexpr float s_MaxDescriptionWidth = 90.f;
		constexpr const char* s_SettingsGridColumnsID = "##SettingsGrid";
		float s_LabelColumnRatio = 0.4f;
		bool s_bFirstSettingsRow = true;

		void BeginSettingsGrid(const char* id)
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y * 2.f));
			UI::PushPropertyMaxValueWidth(ImGui::GetFontSize() * s_MaxValueWidth);
			ImGui::PushID(id);

			// Set before the columns begin, since that's when their clip rects are calculated. On their first frame, they're created by `BeginColumns`
			ImGuiOldColumns* columns = ImGui::FindOrCreateColumns(ImGui::GetCurrentWindow(), ImGui::GetColumnsID(s_SettingsGridColumnsID, 2));
			const bool bInitialized = columns->Columns.Size == 3;
			if (bInitialized)
				columns->Columns[1].OffsetNorm = s_LabelColumnRatio;

			ImGui::BeginColumns(s_SettingsGridColumnsID, 2);
			if (!bInitialized)
				columns->Columns[1].OffsetNorm = s_LabelColumnRatio;

			s_bFirstSettingsRow = true;
		}

		void EndSettingsGrid()
		{
			ImGuiOldColumns* columns = ImGui::GetCurrentWindow()->DC.CurrentColumns;
			ImGui::EndColumns();
			if (columns->IsBeingResized)
				s_LabelColumnRatio = columns->Columns[1].OffsetNorm;

			ImGui::PopID();
			UI::PopPropertyMaxValueWidth();
			ImGui::PopStyleVar();
		}

		void SettingsRow()
		{
			if (!s_bFirstSettingsRow)
			{
				// In the middle of the spacing between the rows, across both columns.
				// It's drawn in the background of the columns, otherwise it'd be clipped to the current column
				ImGui::PushColumnsBackground();
				const float windowX = ImGui::GetWindowPos().x; // Column offsets are relative to the window
				const float y = IM_TRUNC(ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y * 0.5f);
				ImGui::GetWindowDrawList()->AddLine(ImVec2(windowX + ImGui::GetColumnOffset(0), y), ImVec2(windowX + ImGui::GetColumnOffset(2), y),
					ImGui::GetColorU32(ImGuiCol_Separator, 0.5f));
				ImGui::PopColumnsBackground();
			}
			s_bFirstSettingsRow = false;
		}

		const SectionInfo& GetSectionInfo(ProjectSettingsTab::Section section)
		{
			for (const SectionInfo& info : s_Sections)
				if (info.Value == section)
					return info;

			EG_CORE_ASSERT(false, "Unknown section");
			return s_Sections[0];
		}

		// Returns the image's path relative to the project.
		// An image from outside of the project is copied into its Config folder, so that the project stays self-contained.
		// @copyName. The name of the copy (without an extension)
		std::optional<Path> ImportProjectImage(const Path& image, std::string_view copyName)
		{
			namespace fs = std::filesystem;

			std::error_code error;
			const Path projectPath = fs::weakly_canonical(Project::GetProjectPath(), error);
			const Path imagePath = fs::weakly_canonical(image, error);
			const Path relativePath = imagePath.lexically_relative(projectPath);
			if (!relativePath.empty() && relativePath.is_relative() && *relativePath.begin() != "..")
				return relativePath;

			std::string extension = Utils::AsString(imagePath.extension());
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
			const Path destination = Project::GetConfigPath() / (Utils::AsPath(std::string(copyName)) += extension);

			fs::create_directories(destination.parent_path(), error);
			fs::copy_file(imagePath, destination, fs::copy_options::overwrite_existing, error);
			if (error)
			{
				EG_CORE_ERROR("Failed to copy the image `{}` into the project: {}", imagePath, error.message());
				Application::Get().GetImGuiLayer()->AddMessage("Failed to copy the image into the project. See logs for more details");
				return std::nullopt;
			}

			return fs::relative(destination, Project::GetProjectPath(), error);
		}

		// A property grid row that selects an image file of the project
		// @emptyText. Shown when no image is selected
		bool DrawProjectImageProperty(std::string_view label, Path& image, std::string_view emptyText, std::string_view copyName, std::string_view helpMessage)
		{
			bool bChanged = false;

			ImGui::PushID(label.data(), label.data() + label.size());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::TextUnformatted(label.data(), label.data() + label.size());
			if (!helpMessage.empty())
			{
				ImGui::SameLine();
				UI::HelpMarker(helpMessage);
			}
			ImGui::NextColumn();

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			if (image.empty())
			{
				ImGui::TextDisabled("%.*s", int(emptyText.size()), emptyText.data());
			}
			else
			{
				std::error_code error;
				const Path absolutePath = Project::GetProjectPath() / image;
				const bool bExists = std::filesystem::is_regular_file(absolutePath, error);
				const std::string text = Utils::AsString(image);
				if (bExists)
					ImGui::TextUnformatted(text.c_str());
				else
					ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1.f), "%s (missing)", text.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", bExists ? Utils::AsString(std::filesystem::absolute(absolutePath, error)).c_str() : "The file doesn't exist");
			}

			if (ImGui::Button("Browse..."))
			{
				const Path selected = FileDialog::OpenFile(FileDialog::SPLASH_SCREEN_FILTER, Project::GetProjectPath());
				if (!selected.empty())
				{
					if (std::optional<Path> relativePath = ImportProjectImage(selected, copyName))
					{
						image = *relativePath;
						bChanged = true;
					}
				}
			}

			ImGui::SameLine();
			const bool bCanClear = !image.empty();
			if (!bCanClear)
				UI::PushItemDisabled();
			if (ImGui::Button("Clear"))
			{
				image.clear();
				bChanged = true;
			}
			if (!bCanClear)
				UI::PopItemDisabled();

			ImGui::PopID();
			ImGui::NextColumn();
			return bChanged;
		}
	}

	ProjectSettingsTab::~ProjectSettingsTab()
	{
		EditorFrames::RemoveTab(this);
	}

	void ProjectSettingsTab::DrawTabMenus()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Close"))
				CloseTab();

			ImGui::EndMenu();
		}
	}

	bool ProjectSettingsTab::OnImGuiRender()
	{
		if (m_bCloseRequested)
			return false;

		EditorFrames::TabContentScope tabScope(this);

		// Its size when it's dragged out into its own window
		ImGui::SetNextWindowSize(ImVec2(960.f, 640.f), ImGuiCond_FirstUseEver);
		EditorFrames::PrepareTabWindow(this);

		bool bOpened = true;
		if (ImGui::Begin(m_WindowName.c_str(), &bOpened, EditorFrames::TabWindowFlags))
		{
			DrawSectionList();
			ImGui::SameLine();
			DrawSelectedSection();
		}
		ImGui::End();

		return bOpened && !m_bCloseRequested;
	}

	void ProjectSettingsTab::DrawSectionList()
	{
		const float width = ImGui::GetFontSize() * 13.f;
		if (ImGui::BeginChild("##Sections", ImVec2(width, 0.f), ImGuiChildFlags_Borders))
		{
			UI::PushFontBold();
			ImGui::TextUnformatted("Project");
			UI::PopFont();
			ImGui::Spacing();

			for (const SectionInfo& info : s_Sections)
			{
				if (ImGui::Selectable(info.Name, m_SelectedSection == info.Value))
					m_SelectedSection = info.Value;
			}
		}
		ImGui::EndChild();
	}

	void ProjectSettingsTab::DrawSelectedSection()
	{
		if (ImGui::BeginChild("##SelectedSection", ImVec2(0.f, 0.f), ImGuiChildFlags_AlwaysUseWindowPadding))
		{
			const SectionInfo& info = GetSectionInfo(m_SelectedSection);

			UI::PushFontHeader2();
			ImGui::TextUnformatted(info.Name);
			UI::PopFont();
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + glm::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * s_MaxDescriptionWidth));
			ImGui::TextDisabled("%s", info.Description);
			ImGui::PopTextWrapPos();
			ImGui::TextDisabled("Saved in %s", Utils::AsString(Project::GetProjectFilePath().filename()).c_str());
			ImGui::Separator();
			ImGui::Spacing();

			const bool bRuntime = EditorLayer::Get()->GetEditorState() != EditorState::Edit;
			if (bRuntime)
				UI::PushItemDisabled();

			bool bChanged = false;
			switch (m_SelectedSection)
			{
				case Section::General:         bChanged = DrawGeneralSection();         break;
				case Section::CollisionGroups: bChanged = DrawCollisionGroupsSection(); break;
				case Section::Navigation:      bChanged = DrawNavigationSection();      break;
				case Section::UIScale:         bChanged = DrawUIScaleSection();         break;
				case Section::SplashScreen:    bChanged = DrawSplashScreenSection();    break;
			}

			if (bRuntime)
				UI::PopItemDisabled();

			if (bChanged)
				Project::Save();
		}
		ImGui::EndChild();
	}

	bool ProjectSettingsTab::DrawGeneralSection()
	{
		const ProjectInfo& projectInfo = Project::GetProjectInfo();
		Ref<AssetScene> startupScene = projectInfo.GameStartupScene;
		glm::uvec3 version = projectInfo.Version;
		bool bPauseWhenMinimized = projectInfo.bPauseWhenMinimized;
		bool bChanged = false;

		BeginSettingsGrid("ProjectSettingsGeneral");
		SettingsRow();
		if (EditorResources::DrawAssetSelection("Game startup scene", startupScene, "If 'None' is selected, an empty scene will be opened"))
		{
			Project::SetStartupScene(startupScene);
			bChanged = true;
		}

		SettingsRow();
		if (UI::PropertyDrag("Version", version, 1, 0, 0, "Major - Minor - Patch"))
		{
			Project::SetVersion(version);
			bChanged = true;
		}

		SettingsRow();
		if (UI::Property("Pause when minimized", bPauseWhenMinimized, "If set, the game doesn't update while its window is minimized (also when it's played in the editor). "
			"Otherwise, it keeps running and updating the systems (such as scripts, physics, animations, audio), but nothing is rendered, and it updates at a lower rate"))
		{
			Project::SetPauseWhenMinimized(bPauseWhenMinimized);
			bChanged = true;
		}
		EndSettingsGrid();

		return bChanged;
	}

	bool ProjectSettingsTab::DrawCollisionGroupsSection()
	{
		bool bChanged = false;

		BeginSettingsGrid("ProjectSettingsCollisionGroups");
		auto userGroups = Project::GetUserCollisionGroups();
		for (auto it = userGroups.begin(); it != userGroups.end(); ++it)
		{
			auto& name = it->first;
			auto& mask = it->second;

			ImGui::PushID(mask);
			SettingsRow();
			if (ImGui::Button("Delete"))
			{
				Project::RemoveUserCollisionGroup(mask);
				EditorLayer::Get()->OnCollisionGroupsChanged();
				bChanged = true;
			}
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			if (UI::PropertyText("Name", name))
			{
				Project::RenameUserCollisionGroup(mask, name);
				bChanged = true;
			}
			ImGui::PopID();
		}
		EndSettingsGrid();
		ImGui::Separator();

		const bool bCanAdd = Project::CanAddUserCollisionGroup();
		if (!bCanAdd)
			UI::PushItemDisabled();
		if (ImGui::Button("Add Collision Group"))
		{
			Project::AddUserCollisionGroup("New Group");
			EditorLayer::Get()->OnCollisionGroupsChanged();
			bChanged = true;
		}
		if (!bCanAdd)
			UI::PopItemDisabled();

		return bChanged;
	}

	bool ProjectSettingsTab::DrawNavigationSection()
	{
		bool bChanged = false;
		bool bScenesAffected = false;

		BeginSettingsGrid("ProjectSettingsNavigation");
		SettingsRow();
		uint32_t defaultID = Project::GetDefaultNavAgentTypeID();
		if (UI::NavAgentTypeCombo("Default Agent Type", defaultID, false, "Used by navigation queries and by entities without a crowd agent, unless a script picks another type"))
		{
			Project::SetDefaultNavAgentType(defaultID);
			bChanged = true;
		}
		EndSettingsGrid();
		ImGui::Separator();

		uint32_t typeToRemove = AINavigation::InvalidAgentTypeID;
		const bool bCanRemove = Project::CanRemoveNavAgentType();

		// Copied, since changing them changes the project's vector
		const std::vector<AINavigation::AgentType> agentTypes = Project::GetNavAgentTypes();
		for (const AINavigation::AgentType& type : agentTypes)
		{
			ImGui::PushID(int(type.ID));
			const std::string label = type.Name + (type.ID == defaultID ? " (Default)" : "");
			if (UI::PushTreeNode(label, true, true, "", type.ID))
			{
				BeginSettingsGrid("ProjectSettingsAgentType");

				SettingsRow();
				UI::Text("ID", std::to_string(type.ID), "It never changes, and it's never reused by another type. Components and scripts store it");

				SettingsRow();
				auto pendingIt = m_PendingAgentTypeNames.find(type.ID);
				std::string name = pendingIt != m_PendingAgentTypeNames.end() ? pendingIt->second : type.Name;
				if (UI::PropertyText("Name", name, "Must be unique and not empty. Scripts can look agent types up by name"))
				{
					if (Project::RenameNavAgentType(type.ID, name))
					{
						m_PendingAgentTypeNames.erase(type.ID);
						bChanged = true;
					}
					else
						m_PendingAgentTypeNames[type.ID] = name;
				}
				if (m_PendingAgentTypeNames.contains(type.ID))
				{
					// Part of the Name row, so there's no line before it
					UI::Text("", "The name is empty or used by another type. It isn't applied");
				}

				AINavigation::AgentType edited = type;
				bool bEdited = false;
				SettingsRow();
				if (UI::PropertyColor("Debug Color", edited.DebugColor, false, "The color of its nav meshes when they're drawn"))
				{
					bEdited = true;
					bChanged = true;
				}

				auto SizeProperty = [&bEdited, &bChanged, &bScenesAffected](const char* label, float& value, float speed, float max, const char* help)
				{
					SettingsRow();
					bEdited |= UI::PropertyDrag(label, value, speed, 0.f, max, help);
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						bChanged = true;
						bScenesAffected = true;
					}
				};
				SizeProperty("Radius", edited.Radius, 0.01f, 0.f, "The distance to erode/shrink the walkable area of the heightfield away from obstructions");
				SizeProperty("Height", edited.Height, 0.01f, 0.f, "Minimum floor to 'ceiling' height that will still allow the floor area to be considered walkable");
				SizeProperty("Max Climb", edited.MaxClimb, 0.01f, 0.f, "Maximum ledge height that is considered to still be traversable");
				SizeProperty("Max Slope", edited.MaxSlope, 1.f, 90.f, "Degrees. The maximum slope that is considered walkable");
				if (bEdited)
					Project::SetNavAgentType(edited);

				EndSettingsGrid();

				if (!bCanRemove)
					UI::PushItemDisabled();
				if (ImGui::Button("Delete"))
					typeToRemove = type.ID;
				if (!bCanRemove)
				{
					UI::PopItemDisabled();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("At least one agent type must exist");
				}

				UI::PopTreeNode();
			}
			ImGui::PopID();
		}

		if (typeToRemove != AINavigation::InvalidAgentTypeID && Project::RemoveNavAgentType(typeToRemove))
		{
			m_PendingAgentTypeNames.erase(typeToRemove);
			bChanged = true;
			bScenesAffected = true;
		}

		ImGui::Separator();

		const bool bCanAdd = Project::CanAddNavAgentType();
		if (!bCanAdd)
			UI::PushItemDisabled();

		const std::string addLabel = "Add Agent Type (" + std::to_string(agentTypes.size()) + " / " + std::to_string(AINavigation::MaxAgentTypes) + ")";
		if (ImGui::Button(addLabel.c_str()))
		{
			if (Project::AddNavAgentType("New Agent Type") != AINavigation::InvalidAgentTypeID)
				bChanged = true;
		}

		if (!bCanAdd)
			UI::PopItemDisabled();

		if (bScenesAffected)
			EditorLayer::Get()->OnNavAgentTypesChanged();

		return bChanged;
	}

	bool ProjectSettingsTab::DrawUIScaleSection()
	{
		UIScaleSettings uiScale = Project::GetProjectInfo().UIScale;
		bool bChanged = false;

		BeginSettingsGrid("ProjectSettingsUIScale");

		SettingsRow();
		bChanged |= UI::PropertyDrag("Reference Resolution", uiScale.ReferenceResolution, 1.f, 1, 16384,
			"Resolution the 2D UI is designed for. Offsets, sizes & font sizes of Text2D and Image2D are in pixels of this resolution");

		SettingsRow();
		bChanged |= UI::ComboEnum("Mode", uiScale.Mode,
			"How the UI is scaled when the viewport's aspect ratio differs from the reference resolution's.\n"
			"Match Width Or Height - follows the viewport's width, its height, or a blend of them.\n"
			"Expand - the whole reference resolution always fits into the viewport. The viewport has extra space along one axis.\n"
			"Shrink - the reference resolution always covers the viewport. Some of it is outside of the viewport along one axis");

		if (uiScale.Mode == UIScaleMode::MatchWidthOrHeight)
		{
			SettingsRow();
			bChanged |= UI::PropertySlider("Match Width/Height", uiScale.MatchWidthOrHeight, 0.f, 1.f,
				"0 - the UI is scaled with the viewport's width\n1 - with the viewport's height\nValues in between blend them");
		}
		EndSettingsGrid();

		if (bChanged)
		{
			uiScale.ReferenceResolution = glm::max(uiScale.ReferenceResolution, glm::uvec2(1u));
			Project::SetUIScaleSettings(uiScale);
		}

		return bChanged;
	}

	bool ProjectSettingsTab::DrawSplashScreenSection()
	{
		const ProjectInfo& projectInfo = Project::GetProjectInfo();
		GameSplashScreenSettings settings = projectInfo.SplashScreen;
		bool bChanged = false;

		BeginSettingsGrid("ProjectSettingsSplashScreen");
		SettingsRow();
		bChanged |= DrawProjectImageProperty("Image", settings.Image, "Default", "SplashScreen",
			"Replaces the default background. The splash screen takes the image's aspect ratio.\n"
			"An image from outside of the project is copied into the project's Config folder");
		SettingsRow();
		bChanged |= DrawProjectImageProperty("Icon", settings.Icon, "None", "GameIcon",
			"Drawn next to the title, and used as the taskbar icon of the splash screen.\n"
			"It's also the icon of the game's window and of the game's executable.\n"
			"An image from outside of the project is copied into the project's Config folder");

		SettingsRow();
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::TextUnformatted("Title");
			ImGui::SameLine();
			UI::HelpMarker("If it's empty, the project's name is used");
			ImGui::NextColumn();
			ImGui::PushItemWidth(glm::min(ImGui::GetContentRegionAvail().x, UI::GetPropertyMaxValueWidth()));
			bChanged |= UI::InputTextWithHint("##SplashScreenTitle", settings.Title, projectInfo.Name);
			ImGui::PopItemWidth();
			ImGui::NextColumn();
		}
		SettingsRow();
		bChanged |= UI::PropertyText("Subtitle", settings.Subtitle);
		SettingsRow();
		bChanged |= UI::Property("Text Over Image", settings.bShowTextOverImage,
			"Draw the icon, title & subtitle over the custom image. Usually, an image already contains the title, so they aren't drawn over it.\n"
			"Without a custom image, they're always drawn");
		SettingsRow();
		bChanged |= UI::PropertyColor("Title Color", settings.Colors.Title);
		SettingsRow();
		bChanged |= UI::PropertyColor("Subtitle Color", settings.Colors.Subtitle);
		SettingsRow();
		bChanged |= UI::PropertyColor("Progress Bar Color", settings.Colors.Progress);
		EndSettingsGrid();
		ImGui::Separator();

		if (ImGui::Button("Preview"))
		{
			Project::SetSplashScreenSettings(settings);
			EditorLayer::Get()->PreviewSplashScreen();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Shows the splash screen for a few seconds, exactly as the game will show it");

		ImGui::SameLine();
		const bool bDefaultColors = settings.Colors == SplashScreenColors{};
		if (bDefaultColors)
			UI::PushItemDisabled();
		if (ImGui::Button("Reset Colors"))
		{
			settings.Colors = SplashScreenColors{};
			bChanged = true;
		}
		if (bDefaultColors)
			UI::PopItemDisabled();

		if (bChanged)
			Project::SetSplashScreenSettings(settings);

		return bChanged;
	}
}
