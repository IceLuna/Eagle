#include "EditorLayer.h"

#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	static void BeginDocking();
	static void EndDocking();

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{}

	void EditorLayer::OnAttach()
	{
		m_Texture = Texture2D::Create("assets/textures/test.png");
		m_SpriteSheet = Texture2D::Create("assets/textures/RPGpack_sheet_2X.png");

		m_StairTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 7, 6 }, { 128, 128 });
		m_BarrelTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 8, 2 }, { 128, 128 });
		m_TreeTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 2, 1 }, { 128, 128 }, { 1, 2 });

		m_Framebuffer = Framebuffer::Create(FramebufferSpecification((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y));

		m_ActiveScene = MakeRef<Scene>();
	#if 0
		m_CameraEntity = m_ActiveScene->CreateEntity("Main Camera");
		m_CameraEntity.AddComponent<CameraComponent>().Primary = true;
		m_CameraEntity.AddComponent<NativeScriptComponent>().Bind<CameraController>();

		m_SquareEntity = m_ActiveScene->CreateEntity("Front");

		{
			SpriteComponent& sprite = m_ActiveScene->CreateEntity("Left").AddComponent<SpriteComponent>();
			sprite.Transform.Translation.x = -0.5f;
			sprite.Transform.Translation.z = -10.5f;
			sprite.Transform.Rotation.y = glm::radians(90.f);
			sprite.Color = {0.7f, 0.8f, 0.2f, 1.f};
		}
		{
			SpriteComponent& sprite = m_ActiveScene->CreateEntity("Right").AddComponent<SpriteComponent>();
			sprite.Transform.Translation.x =  0.5f;
			sprite.Transform.Translation.z = -10.5f;
			sprite.Transform.Rotation.y = glm::radians(90.f);
			sprite.Color = { 0.2f, 0.7f, 0.8f, 1.f };
		}
		{
			SpriteComponent& sprite = m_ActiveScene->CreateEntity("Top").AddComponent<SpriteComponent>();
			sprite.Transform.Translation.y =  0.5f;
			sprite.Transform.Translation.z = -10.5f;
			sprite.Transform.Rotation.x = glm::radians(90.f);
			sprite.Color = { 0.7f, 0.8f, 0.6f, 1.f };
		}
		{
			SpriteComponent& sprite = m_ActiveScene->CreateEntity("Bottom").AddComponent<SpriteComponent>();
			sprite.Transform.Translation.y = -0.5f;
			sprite.Transform.Translation.z = -10.5f;
			sprite.Transform.Rotation.x = glm::radians(90.f);
			sprite.Color = { 0.3f, 0.8f, 0.7f, 1.f };
		}

		SpriteComponent& sprite = m_SquareEntity.AddComponent<SpriteComponent>();
		sprite.Transform.Translation.z = -10.f;
		sprite.Color = { 0.8f, 0.2f, 0.7f, 1.f };

		SceneSerializer ser(m_ActiveScene);
		ser.Serialize("assets/scenes/Example.eagle");
	#endif
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDetach()
	{}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		EG_PROFILE_FUNCTION();
		m_Ts = ts;

		if (m_NewViewportSize != m_CurrentViewportSize) //If size changed, resize framebuffer
		{
			m_CurrentViewportSize = m_NewViewportSize;
			Renderer::WindowResized((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			m_Framebuffer->Resize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		}

		m_Framebuffer->Bind();
		{
			Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			Renderer::Clear();

			Renderer2D::ResetStats();
			{
				EG_PROFILE_SCOPE("EditorLayer::Draw Scene");
				m_ActiveScene->OnUpdate(ts);
			}
		}
		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnEvent(Eagle::Event& e)
	{
		if (m_ViewportHovered)
		{
			m_ActiveScene->OnEvent(e);
		}
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_PROFILE_FUNCTION();

		BeginDocking();

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New", "Ctrl+N"))
				{
					NewScene();
				}
				if (ImGui::MenuItem("Open...", "Ctrl+O"))
				{
					OpenScene();
				}
				if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S"))
				{
					SaveSceneAs();
				}
				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
					Eagle::Application::Get().SetShouldClose(true);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		auto stats = Renderer2D::GetStats();
		bool bVSync = Application::Get().GetWindow().IsVSync();
		ImGui::Begin("Stats");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetVertexCount());
		ImGui::Text("Indices: %d", stats.GetIndexCount());
		ImGui::Text("Frame Time: %.6fms", (float)m_Ts);
		ImGui::Text("FPS: %d", int(1.f/m_Ts));
		if (ImGui::Checkbox("VSync", &bVSync))
		{
			Application::Get().GetWindow().SetVSync(bVSync);
		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);
		m_ViewportHovered = ImGui::IsWindowHovered();
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
		m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

		uint64_t textureID = 0;
		if (m_DepthAttachment)
		{
			textureID = (uint64_t)m_Framebuffer->GetDepthAttachment();
		}
		else
		{
			textureID = (uint64_t)m_Framebuffer->GetColorAttachment();
		}
		ImGui::Image((void*)textureID, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y}, { 0, 1 }, { 1, 0 });
		
		ImGui::End();
		ImGui::PopStyleVar();

		m_SceneHierarchyPanel.OnImGuiRender();

		EndDocking();
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		//Shortcuts
		if (e.GetRepeatCount() > 0)
			return false;

		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode())
		{
			case Key::N:
				if (control)
					NewScene();
				break;

			case Key::O:
				if (control)
					OpenScene();
				break;

			case Key::S:
				if (control && shift)
					SaveSceneAs();
				break;
		}

		return false;
	}

	void EditorLayer::NewScene()
	{
		m_ActiveScene = MakeRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialog::OpenFile("Eagle Scene (*.eagle)\0*.eagle\0");
		if (!filepath.empty())
		{
			m_ActiveScene = MakeRef<Scene>();
			m_ActiveScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_ActiveScene);

			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(filepath);
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialog::SaveFile("Eagle Scene (*.eagle)\0*.eagle\0");
		if (!filepath.empty())
		{
			EG_CORE_INFO("Saving Scene at '{0}'", filepath);
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filepath);
		}
		else
		{
			EG_CORE_ERROR("Couldn't save scene {0}", filepath);
		}
	}

	static void BeginDocking()
	{
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoCloseButton;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
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
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
	}

	static void EndDocking()
	{
		ImGui::End(); //Docking
	}
}
