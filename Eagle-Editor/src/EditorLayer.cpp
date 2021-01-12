#include "EditorLayer.h"

#include <Platform/OpenGL/OpenGLShader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Eagle/Core/Core.h>

#include <imgui/imgui.h>

namespace Eagle
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer"),
		m_CameraController(1280.f / 720.f)
	{}

	void EditorLayer::OnAttach()
	{
		m_Texture = Texture2D::Create("assets/textures/test.png");
		m_SpriteSheet = Texture2D::Create("assets/textures/RPGpack_sheet_2X.png");

		m_StairTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 7, 6 }, { 128, 128 });
		m_BarrelTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 8, 2 }, { 128, 128 });
		m_TreeTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 2, 1 }, { 128, 128 }, { 1, 2 });

		m_Framebuffer = Framebuffer::Create(FramebufferSpecification((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y));
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
		}

		m_CameraController.OnUpdate(ts);

		{
			EG_PROFILE_SCOPE("EditorLayer::Clear");
			m_Framebuffer->Bind();
			Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			Renderer::Clear();
		}

		Renderer2D::ResetStats();
		{
			EG_PROFILE_SCOPE("EditorLayer::Draw");
			static float rotation = 0.f;
			rotation += 45.f * ts;
			Renderer2D::BeginScene(m_CameraController.GetCamera());

			Renderer2D::DrawRotatedQuad({ -1.0f,  -0.7f }, { 0.7f, 0.7f }, glm::radians(rotation), m_SquareColor1);
			Renderer2D::DrawQuad({ -0.5f,  0.20f, 0.9f }, { 0.7f, 0.7f }, m_SquareColor1);
			Renderer2D::DrawQuad({ 0.5f, -0.25f }, { 0.3f, 0.8f }, m_SquareColor2);
			Renderer2D::DrawQuad({ 0.2f,  0.2f, 0.2f }, { 0.8f, 0.8f }, m_Texture, textureProps);

			Renderer2D::DrawRotatedQuad({ -2.f,  1.5f, 0.1f }, { 1.f, 1.f }, glm::radians(rotation), m_StairTexture, textureProps);
			Renderer2D::DrawQuad({ -2.f,  0.f, 0.1f }, { 1.f, 1.f }, m_BarrelTexture, textureProps);
			Renderer2D::DrawQuad({ -2.f,  -1.5f, 0.1f }, { 1.f, 2.f }, m_TreeTexture, textureProps);

			Renderer2D::DrawRotatedQuad({ 0.2f,  -0.8f, 0.1f }, { 0.8f, 0.8f }, glm::radians(rotation), m_Texture, textureProps);

			Renderer2D::EndScene();

			m_Framebuffer->Unbind();
		}
	}

	void EditorLayer::OnEvent(Eagle::Event& e)
	{
		m_CameraController.OnEvent(e);
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_PROFILE_FUNCTION();

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
					Eagle::Application::Get().SetShouldClose(true);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		auto stats = Renderer2D::GetStats();
		ImGui::Begin("Stats");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetVertexCount());
		ImGui::Text("Indices: %d", stats.GetIndexCount());
		ImGui::Text("Frame Time: %.3fms", (float)m_Ts);
		ImGui::End();

		ImGui::Begin("Settings");
		ImGui::ColorEdit4("Square Color 1", glm::value_ptr(m_SquareColor1));
		ImGui::ColorEdit4("Square Color 2", glm::value_ptr(m_SquareColor2));
		ImGui::SliderFloat("Texture Opacity", &textureProps.Opacity, 0.0f, 1.0f);
		ImGui::SliderFloat("Texture Tiling", &textureProps.TilingFactor, 0.0f, 5.0f);
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin("Viewport");
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
		m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

		uint64_t textureID = (uint64_t)m_Framebuffer->GetColorAttachment();
		ImGui::Image((void*)textureID, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y}, { 0, 1 }, { 1, 0 });
		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::End(); //Docking
	}
}