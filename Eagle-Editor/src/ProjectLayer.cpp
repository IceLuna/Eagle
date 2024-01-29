#include "ProjectLayer.h"
#include "EditorLayer.h"

#include "Eagle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	static void BeginDocking();
	static void EndDocking();

	void ProjectLayer::OnAttach()
	{
		m_NoTabBar.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoTabBar;

		// Enable VSync during `ProjectLayer`
		Application& app = Application::Get();
		Window& window = app.GetWindow();
		window.SetWindowTitle("Eagle Projects");
		bWasVSync = window.IsVSync();
		if (!bWasVSync)
			window.SetVSync(true);
		
		const auto& corePath = app.GetCorePath();
		const auto basePath = corePath / "assets/textures/Editor";
		m_OpenProjectIcon = Texture2D::Create(basePath / "openproject.png");
		m_CreateProjectIcon = Texture2D::Create(basePath / "newproject.png");
		m_UserProjectIcon = Texture2D::Create(basePath / "userproject.png");

		const Path recentProjectsFile = corePath / "Saved/RecentProjects";
		if (std::filesystem::exists(recentProjectsFile))
		{
			YAML::Node node = YAML::LoadFile(recentProjectsFile.u8string());
			if (auto recentProjectsNode = node["RecentProjects"])
			{
				for (auto& projectNode : recentProjectsNode)
					AddRecentProject(projectNode.as<std::string>());
			}
		}
	}
	
	void ProjectLayer::OnDetach()
	{
		auto& app = Application::Get();
		if (!bWasVSync)
			app.GetWindow().SetVSync(bWasVSync);

		const auto savedDir = app.GetCorePath() / "Saved";
		if (!std::filesystem::exists(savedDir))
			std::filesystem::create_directories(savedDir);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;

		for (const auto& recentProject : m_RecentProjects)
			out << recentProject.string();

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

		BeginDocking();

		ImGui::SetNextWindowClass(&m_NoTabBar);
		if (ImGui::Begin("Project Selector", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
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

		ImGui::End();

		EndDocking();
	}

	void ProjectLayer::OpenEditor()
	{
		const auto& projectInfo = Project::GetProjectInfo();
		const auto path = projectInfo.BasePath / (projectInfo.Name + Project::GetExtension());
		AddRecentProject(std::filesystem::absolute(path));

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

			ImGui::Text("Location: %s", m_NewProjectPath.u8string().c_str());
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

		ImGui::SetWindowFontScale(1.5f);

		if (UI::ImageButtonWithText(m_CreateProjectIcon, "Create project", { size, size }, -itemSpacing))
			m_DrawCreateProjectPopup = true;

		ImGui::SameLine();

		if (UI::ImageButtonWithText(m_OpenProjectIcon, "Open project", { size, size }, -itemSpacing))
		{
			const Path filepath = FileDialog::OpenFile(FileDialog::PROJECT_FILTER);
			if (!filepath.empty())
			{
				if (Project::Open(filepath))
					OpenEditor();
			}
		}
		ImGui::SetWindowFontScale(1.f);
	}

	void ProjectLayer::DrawRecentProjects()
	{
		ImGui::SetWindowFontScale(2.f);
		ImGui::Text("Recent projects");
		ImGui::SetWindowFontScale(1.1f);

		if (m_RecentProjects.empty())
		{
			ImGui::SetWindowFontScale(1.f);
			return;
		}

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

			const std::string name = path.stem().string();
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

		ImGui::SetWindowFontScale(1.f);

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

	static void BeginDocking()
	{
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoCloseButton;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = (true ? 0u : ImGuiWindowFlags_MenuBar) | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Project_DockspaceWindow", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("Project_Dockspace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
	}

	static void EndDocking()
	{
		ImGui::End(); //Docking
	}
}
