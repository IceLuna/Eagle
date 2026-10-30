#include "ProjectLayer.h"
#include "EditorLayer.h"
#include "Frames/TitleBar.h"

#include "Eagle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	void ProjectLayer::OnAttach()
	{
		// Enable VSync during `ProjectLayer`
		Application& app = Application::Get();
		Window& window = app.GetWindow();
		window.SetWindowTitle("Eagle Projects");
		bWasVSync = window.IsVSync();
		if (!bWasVSync)
			window.SetVSync(true);

		// The window draws its own title bar
		window.SetTitleBarHitTestCallback(TitleBar::MakeHitTestCallback(ImGui::GetMainViewport()->ID));
		
		const auto& corePath = app.GetCorePath();
		const auto basePath = corePath / "assets/textures/Editor";
		m_OpenProjectIcon = Texture2D::Create(basePath / "openproject.png");
		m_CreateProjectIcon = Texture2D::Create(basePath / "newproject.png");
		m_UserProjectIcon = Texture2D::Create(basePath / "userproject.png");
		m_EditorIcon = Texture2D::Create(basePath / "icon.png");

		const Path recentProjectsFile = corePath / "Saved/RecentProjects";
		if (std::filesystem::exists(recentProjectsFile))
		{
			YAML::Node node = YAML::Load(FileSystem::ReadText(recentProjectsFile));
			if (auto recentProjectsNode = node["RecentProjects"])
			{
				for (const auto& projectNode : recentProjectsNode)
					AddRecentProject(projectNode.as<std::string>());
			}
		}
	}
	
	void ProjectLayer::OnDetach()
	{
		auto& app = Application::Get();
		if (!bWasVSync)
			app.GetWindow().SetVSync(bWasVSync);
		app.GetWindow().SetTitleBarHitTestCallback(nullptr);

		const auto savedDir = app.GetCorePath() / "Saved";
		if (!std::filesystem::exists(savedDir))
			std::filesystem::create_directories(savedDir);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;

		for (const auto& recentProject : m_RecentProjects)
			out << Utils::AsString(recentProject);

		YAML::EndSeq;
		YAML::EndMap;

		std::ofstream fout(savedDir / "RecentProjects");
		fout << out.c_str();
		fout.close();
	}
	
	void ProjectLayer::OnUpdate(Timestep ts)
	{
	}
	
	void ProjectLayer::OnEvent(Event& e)
	{
	}
	
	void ProjectLayer::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("ProjectLayer. UI");

		TitleBar::NewFrame();

		// A window that covers the main window: the title bar, and the project selector below it
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, TitleBar::GetBackgroundColor()); // The edges around the content are a part of the frame
		ImGui::Begin("##ProjectsFrame", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);

		ImGuiWindow* frame = ImGui::GetCurrentWindow();
		const Window& window = Application::Get().GetWindow();
		float titleBarHeight = 0.f;
		if (window.HasCustomTitleBar() && !window.IsFullscreen())
		{
			TitleBarDesc titleBar;
			titleBar.Title = window.GetWindowTitle();
			titleBar.Icon = m_EditorIcon;
			titleBar.OnClose = &TitleBar::RequestAppClose;
			TitleBar::Draw(titleBar);
			titleBarHeight = TitleBar::GetHeight();
		}

		// The edges of the window resize it, so the content is inset by them
		const float border = TitleBar::GetResizeBorder(viewport);
		ImGui::SetCursorScreenPos(ImVec2(frame->Pos.x + border, frame->Pos.y + titleBarHeight));
		const ImVec2 contentSize(ImMax(1.f, frame->Size.x - border * 2.f), ImMax(1.f, frame->Size.y - titleBarHeight - border));

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
		const bool bContentVisible = ImGui::BeginChild("Project Selector", contentSize, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleColor();
		if (bContentVisible)
		{
			DrawCreateOpenButtons();

			ImGui::Separator();
			ImGui::Separator();

			DrawRecentProjects();

			if (m_DrawCreateProjectPopup)
			{
				if (auto button = DrawCreateProjectPopup("Eagle Projects", "Project name"); button != UI::ButtonType::None)
				{
					if (button == UI::ButtonType::OK)
					{
						ProjectInfo info;
						info.BasePath = m_NewProjectPath;
						info.Name = m_NewProjectName;

						Project::Create(info);
						if (Project::Open(m_NewProjectPath / (m_NewProjectName + Project::GetExtension())))
							OpenEditor();
						m_DrawCreateProjectPopup = false;
					}
					else if (button == UI::ButtonType::Cancel)
					{
						m_DrawCreateProjectPopup = false;
					}
				}
			}
		}
		ImGui::EndChild();

		ImGui::End();
	}

	void ProjectLayer::OpenEditor()
	{
		const auto path = Project::GetProjectFilePath();
		AddRecentProject(std::filesystem::absolute(path));

		Application& app = Application::Get();
		SplashScreenDesc splashScreen = app.GetSplashScreenDesc();
		splashScreen.Subtitle = Project::GetProjectInfo().Name;
		app.ShowSplashScreen(splashScreen);

		Application::Get().CallNextFrame([projectLayer = shared_from_this()]()
		{
			Application& app = Application::Get();
			app.PopLayer(projectLayer);
			app.PushLayer(MakeRef<EditorLayer>());
		});
	}

	UI::ButtonType ProjectLayer::DrawCreateProjectPopup(const std::string_view title, const std::string_view hint)
	{
		UI::ButtonType buttons = UI::ButtonType::OKCancel;

		UI::ButtonType pressedButton = UI::ButtonType::None;
		ImGui::OpenPopup(title.data());

		// Always center this window when appearing
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			char buf[128] = { 0 };
			memcpy_s(buf, sizeof(buf), m_NewProjectName.c_str(), m_NewProjectName.size());
			if (ImGui::InputTextWithHint("##MyInputPopup", hint.data(), buf, sizeof(buf)))
				m_NewProjectName = buf;

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);

			const float spacing = ImGui::GetStyle().FramePadding.x;

			ImGui::Text("Folder");
			ImGui::SameLine(0, spacing);
			UI::HelpMarker("Project files will be put into this folder");
			ImGui::SameLine(0, spacing);
			ImGui::Text(": %s", Utils::AsString(m_NewProjectPath).c_str());
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.f);
			if (ImGui::Button("Browse"))
				m_NewProjectPath = FileDialog::OpenFolder();

			ImGui::Separator();

			//Manually drawing OK because it needs to be disabled if input is empty.
			if (HasFlags(buttons, UI::ButtonType::OK))
			{
				const bool bDisable = m_NewProjectName.size() == 0 ||
					!std::filesystem::exists(m_NewProjectPath) || // Disable if a folder doesn't exist
					std::filesystem::exists(m_NewProjectPath / (m_NewProjectName + Project::GetExtension())); // Disable if a file already exists

				if (bDisable)
					UI::PushItemDisabled();
				if (ImGui::Button("Create", ImVec2(120, 0)))
				{
					pressedButton = UI::ButtonType::OK;
					ImGui::CloseCurrentPopup();
				}
				if (bDisable)
					UI::PopItemDisabled();
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				buttons = UI::ButtonType(buttons & (~UI::ButtonType::OK)); //Removing OK from mask to draw buttons as usual without OK cuz we already drew it
			}
			if (auto pressed = UI::DrawButtons(buttons); pressed != UI::ButtonType::None)
				pressedButton = pressed;

			ImGui::EndPopup();
		}

		return pressedButton;
	}

	void ProjectLayer::DrawCreateOpenButtons()
	{
		constexpr uint32_t buttonsCount = 2;
		const float size = ImGui::GetWindowContentRegionMax().x / 6.f;

		// Center buttons
		const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float totalItemSpacing = itemSpacing * (buttonsCount - 1);
		const float middle = (ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f);
		ImGui::SetCursorPosX(middle - (totalItemSpacing + 0.5f * size * (buttonsCount - 1)));

		UI::PushFontHeader();

		if (UI::ImageButtonWithText(m_CreateProjectIcon, "Create project", { size, size }, true, 1.f, -itemSpacing))
			m_DrawCreateProjectPopup = true;

		ImGui::SameLine();

		if (UI::ImageButtonWithText(m_OpenProjectIcon, "Open project", { size, size }, true, 1.f, -itemSpacing))
		{
			const Path filepath = FileDialog::OpenFile(FileDialog::PROJECT_FILTER);
			if (!filepath.empty())
			{
				if (Project::Open(filepath))
					OpenEditor();
			}
		}
		UI::PopFont();
	}

	void ProjectLayer::DrawRecentProjects()
	{
		UI::PushFontHeader2();
		ImGui::Text("Recent projects");
		UI::PopFont();

		if (m_RecentProjects.empty())
		{
			return;
		}

		UI::PushFontHeader();

		const float windowWidth = ImGui::GetContentRegionAvail().x;
		const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float size = windowWidth / 10.f;
		const float itemWidth = size + itemSpacing * 2.f;

		const int columns = int(windowWidth / itemWidth);
		if (columns > 1)
		{
			ImGui::Columns(columns, nullptr, false);
			ImGui::SetColumnWidth(0, itemWidth);
		}

		int64_t pathToRemoveIndex = -1;
		bool bHoveredAnyItem = false;

		const size_t startIndex = m_RecentProjects.size() - 1;
		for (size_t i = startIndex; ; --i)
		{
			const auto& path = m_RecentProjects[i];
			if (!std::filesystem::exists(path))
			{
				if (i == 0)
					break;
				continue;
			}

			ImGui::PushID(path.c_str());

			const bool bSelected = m_SelectedPath == path;
			if (bSelected)
				UI::PushButtonSelectedStyleColors();

			const std::string name = Utils::AsString(path.stem());
			if (UI::ImageButtonWithText(m_UserProjectIcon, name, { size, size }))
				m_SelectedPath = path;

			if (bSelected)
				UI::PopButtonSelectedStyleColors();

			const bool bHovered = ImGui::IsItemHovered();
			bHoveredAnyItem |= bHovered;

			if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (Project::Open(path))
					OpenEditor();
			}

			if (ImGui::BeginPopupContextItem(name.c_str()))
			{
				if (ImGui::MenuItem("Open"))
				{
					if (Project::Open(path))
						OpenEditor();
				}
				if (ImGui::MenuItem("Remove from recents"))
					pathToRemoveIndex = int64_t(i);

				ImGui::EndPopup();
			}

			ImGui::PopID();
		
			ImGui::NextColumn();
			ImGui::SetColumnWidth(-1, itemWidth);

			if (i == 0)
				break;
		}

		ImGui::Columns(1);

		UI::PopFont();

		if (pathToRemoveIndex >= 0)
			m_RecentProjects.erase(m_RecentProjects.begin() + pathToRemoveIndex);

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !bHoveredAnyItem)
			m_SelectedPath.clear();
	}

	void ProjectLayer::AddRecentProject(const Path& path)
	{
		// Remove existing and push again so that the more recent project is at the end
		auto it = std::find(m_RecentProjects.begin(), m_RecentProjects.end(), path);
		if (it != m_RecentProjects.end())
			m_RecentProjects.erase(it);

		m_RecentProjects.emplace_back(path);
	}
}
