#include "egpch.h"
#include "ProjectSettingsTab.h"

#include "../EditorLayer.h"
#include "../EditorResources.h"

#include "Eagle/Core/Project.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>

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
			{ ProjectSettingsTab::Section::UIScale, "UI Scale", "How the 2D UI (for example, Text2D and Image2D components) is scaled to the resolution of the viewport" },
			{ ProjectSettingsTab::Section::SplashScreen, "Game Splash Screen",
				"Shown while the game is starting, until its first frames are rendered. It doesn't affect the editor, it has its own splash screen" },
		};

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
			ImGui::PushTextWrapPos(0.f);
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

		UI::BeginPropertyGrid("ProjectSettingsGeneral");
		if (EditorResources::DrawAssetSelection("Game startup scene", startupScene, "If 'None' is selected, an empty scene will be opened"))
		{
			Project::SetStartupScene(startupScene);
			bChanged = true;
		}

		if (UI::PropertyDrag("Version", version, 1, 0, 0, "Major - Minor - Patch"))
		{
			Project::SetVersion(version);
			bChanged = true;
		}

		if (UI::Property("Pause when minimized", bPauseWhenMinimized, "If set, the game doesn't update while its window is minimized (also when it's played in the editor). "
			"Otherwise, it keeps running and updating the systems (such as scripts, physics, animations, audio), but nothing is rendered, and it updates at a lower rate"))
		{
			Project::SetPauseWhenMinimized(bPauseWhenMinimized);
			bChanged = true;
		}
		UI::EndPropertyGrid();

		return bChanged;
	}

	bool ProjectSettingsTab::DrawCollisionGroupsSection()
	{
		bool bChanged = false;

		UI::BeginPropertyGrid("ProjectSettingsCollisionGroups");
		auto userGroups = Project::GetUserCollisionGroups();
		for (auto it = userGroups.begin(); it != userGroups.end(); ++it)
		{
			auto& name = it->first;
			auto& mask = it->second;

			ImGui::PushID(mask);
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
		UI::EndPropertyGrid();
		ImGui::Separator();

		const bool bCanAdd = Project::CanAddUserCollisionGroup();
		if (!bCanAdd)
			UI::PushItemDisabled();
		if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			Project::AddUserCollisionGroup("New Group");
			EditorLayer::Get()->OnCollisionGroupsChanged();
			bChanged = true;
		}
		if (!bCanAdd)
			UI::PopItemDisabled();

		return bChanged;
	}

	bool ProjectSettingsTab::DrawUIScaleSection()
	{
		UIScaleSettings uiScale = Project::GetProjectInfo().UIScale;
		bool bChanged = false;

		UI::BeginPropertyGrid("ProjectSettingsUIScale");

		bChanged |= UI::PropertyDrag("Reference Resolution", uiScale.ReferenceResolution, 1.f, 1, 16384,
			"Resolution the 2D UI is designed for. Offsets, sizes & font sizes of Text2D and Image2D are in pixels of this resolution");

		bChanged |= UI::ComboEnum("Mode", uiScale.Mode,
			"How the UI is scaled when the viewport's aspect ratio differs from the reference resolution's.\n"
			"Match Width Or Height - follows the viewport's width, its height, or a blend of them.\n"
			"Expand - the whole reference resolution always fits into the viewport. The viewport has extra space along one axis.\n"
			"Shrink - the reference resolution always covers the viewport. Some of it is outside of the viewport along one axis");

		if (uiScale.Mode == UIScaleMode::MatchWidthOrHeight)
		{
			bChanged |= UI::PropertySlider("Match Width/Height", uiScale.MatchWidthOrHeight, 0.f, 1.f,
				"0 - the UI is scaled with the viewport's width\n1 - with the viewport's height\nValues in between blend them");
		}
		UI::EndPropertyGrid();

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

		UI::BeginPropertyGrid("ProjectSettingsSplashScreen");
		bChanged |= DrawProjectImageProperty("Image", settings.Image, "Default", "SplashScreen",
			"Replaces the default background. The splash screen takes the image's aspect ratio.\n"
			"An image from outside of the project is copied into the project's Config folder");
		bChanged |= DrawProjectImageProperty("Icon", settings.Icon, "None", "GameIcon",
			"Drawn next to the title, and used as the taskbar icon of the splash screen.\n"
			"It's also the icon of the game's window and of the game's executable.\n"
			"An image from outside of the project is copied into the project's Config folder");

		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::TextUnformatted("Title");
			ImGui::SameLine();
			UI::HelpMarker("If it's empty, the project's name is used");
			ImGui::NextColumn();
			ImGui::PushItemWidth(-1);
			bChanged |= UI::InputTextWithHint("##SplashScreenTitle", settings.Title, projectInfo.Name);
			ImGui::PopItemWidth();
			ImGui::NextColumn();
		}
		bChanged |= UI::PropertyText("Subtitle", settings.Subtitle);
		bChanged |= UI::Property("Text Over Image", settings.bShowTextOverImage,
			"Draw the icon, title & subtitle over the custom image. Usually, an image already contains the title, so they aren't drawn over it.\n"
			"Without a custom image, they're always drawn");
		bChanged |= UI::PropertyColor("Title Color", settings.Colors.Title);
		bChanged |= UI::PropertyColor("Subtitle Color", settings.Colors.Subtitle);
		bChanged |= UI::PropertyColor("Progress Bar Color", settings.Colors.Progress);
		UI::EndPropertyGrid();
		ImGui::Separator();

		const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		if (ImGui::Button("Preview", ImVec2(buttonWidth, 0.f)))
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
		if (ImGui::Button("Reset Colors", ImVec2(buttonWidth, 0.f)))
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
