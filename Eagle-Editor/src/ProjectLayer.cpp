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
	}
	
	void ProjectLayer::OnDetach()
	{
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

		if (ImGui::Begin("Project Selector"))
		{
			if (ImGui::Button("Create project"))
			{
				const Path projectFolder = FileDialog::OpenFolder();
				if (!projectFolder.empty())
				{
					ProjectInfo info;
					info.BasePath = projectFolder;
					info.Name = "TestProjectName";

					Project::Create(info);
				}
			}
			if (ImGui::Button("Open project"))
			{
				const Path filepath = FileDialog::OpenFile(FileDialog::PROJECT_FILTER);
				if (!filepath.empty())
				{
					if (Project::Open(filepath))
						OpenEditor();
				}
			}
		}
		ImGui::End();

		EndDocking();
	}

	void ProjectLayer::OpenEditor()
	{
		Application::Get().CallNextFrame([projectLayer = shared_from_this()]()
		{
			Application& app = Application::Get();
			app.PopLayer(projectLayer);
			app.PushLayer(MakeRef<EditorLayer>());
		});
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
