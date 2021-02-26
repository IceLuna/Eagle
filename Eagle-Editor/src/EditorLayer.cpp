#include "EditorLayer.h"

#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Math/Math.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo.h>

namespace Eagle
{
	static void BeginDocking();
	static void EndDocking();

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{}

	void EditorLayer::OnAttach()
	{
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
		m_Texture = Texture2D::Create("assets/textures/test.png");
		m_SpriteSheet = Texture2D::Create("assets/textures/RPGpack_sheet_2X.png");

		m_StairTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 7, 6 }, { 128, 128 });
		m_BarrelTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 8, 2 }, { 128, 128 });
		m_TreeTexture = SubTexture2D::CreateFromCoords(m_SpriteSheet, { 2, 1 }, { 128, 128 }, { 1, 2 });

		FramebufferSpecification fbSpecs;
		fbSpecs.Width = (uint32_t)m_CurrentViewportSize.x;
		fbSpecs.Height = (uint32_t)m_CurrentViewportSize.y;
		fbSpecs.Attachments = {{FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RED_INTEGER}, {FramebufferTextureFormat::Depth}};

		m_Framebuffer = Framebuffer::Create(fbSpecs);

		m_ActiveScene = MakeRef<Scene>();
	
	#if 0
		{
			Entity entity = m_ActiveScene->CreateEntity("Back");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(2.f, 0.f, -15.5f);
			worldTransform.Scale3D = glm::vec3(5.f, 5.f, 1.f);

			entity.AddComponent<SpriteComponent>().Color = { 0.8f, 0.2f, 0.7f, 1.f };
		}

		{
			Entity entity = m_ActiveScene->CreateEntity("Front");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(2.f, 0.f, -5.5f);
			worldTransform.Scale3D = glm::vec3(5.f, 5.f, 1.f);

			entity.AddComponent<SpriteComponent>().Color = { 0.8f, 0.2f, 0.7f, 1.f };
		}

		{
			Entity entity = m_ActiveScene->CreateEntity("Left");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(-0.5f, 0.f, -10.5f);
			worldTransform.Rotation.y = glm::radians(90.f);
			worldTransform.Scale3D = glm::vec3(10.f, 5.f, 1.f);
			
			entity.AddComponent<SpriteComponent>().Color = { 0.7f, 0.8f, 0.2f, 1.f };
		}
		{
			Entity entity = m_ActiveScene->CreateEntity("Right");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(4.5f, 0.f, -10.5f);
			worldTransform.Rotation.y = glm::radians(90.f);
			worldTransform.Scale3D = glm::vec3(10.f, 5.f, 1.f);

			entity.AddComponent<SpriteComponent>().Color = { 0.2f, 0.7f, 0.8f, 1.f };
		}
		{
			Entity entity = m_ActiveScene->CreateEntity("Top");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(2.f, 2.5f, -10.5f);
			worldTransform.Rotation.x = glm::radians(90.f);
			worldTransform.Scale3D = glm::vec3(5.f, 10.f, 1.f);

			entity.AddComponent<SpriteComponent>().Color = { 0.7f, 0.8f, 0.6f, 1.f };
		}
		{
			Entity entity = m_ActiveScene->CreateEntity("Bottom");
			auto& transformComponent = entity.GetComponent<TransformComponent>();
			auto& worldTransform = transformComponent.WorldTransform;

			worldTransform.Translation = glm::vec3(2.f, -2.5f, -10.5f);
			worldTransform.Rotation.x = glm::radians(90.f);
			worldTransform.Scale3D = glm::vec3(5.f, 10.f, 1.f);

			entity.AddComponent<SpriteComponent>().Color = { 0.3f, 0.8f, 0.7f, 1.f };
		}

		SceneSerializer ser(m_ActiveScene);
		ser.Serialize("assets/scenes/Example.eagle");
	#else
		SceneSerializer ser(m_ActiveScene);
		ser.Deserialize("assets/scenes/Example.eagle");
		m_OpenedScene = "assets/scenes/Example.eagle";
		m_WindowTitle = Input::GetWindowTitle();
		Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScene.string());
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
			m_Framebuffer->ClearColorAttachment(2, -1); //2 - RED_INTEGER

			Renderer2D::ResetStats();
			{
				EG_PROFILE_SCOPE("EditorLayer::Draw Scene");
				m_ActiveScene->OnUpdateEditor(ts);
			}
		}

		//Entity Selection
		bool bUsingImGuizmo = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
		if (m_ViewportActive && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
		{
			auto [mx, my] = ImGui::GetMousePos();
			mx -= m_ViewportBounds[0].x;
			my -= m_ViewportBounds[0].y;

			glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			my = viewportSize.y - my;

			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
			{
				int pixelData = m_Framebuffer->ReadPixel(2, mouseX, mouseY); //2 - RED_INTEGER
				m_SceneHierarchyPanel.SetEntitySelected(pixelData);
			}
		}

		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnEvent(Eagle::Event& e)
	{
		m_ActiveScene->OnEventEditor(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_PROFILE_FUNCTION();

		BeginDocking();

		//---------------------------Menu bar---------------------------
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
				ImGui::Separator();
				if (ImGui::MenuItem("Save", "Ctrl+S"))
				{
					SaveScene();
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

		//---------------------------Stats---------------------------
		auto stats = Renderer2D::GetStats();
		bool bVSync = Application::Get().GetWindow().IsVSync();
		ImGui::Begin("Stats");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetVertexCount());
		ImGui::Text("Indices: %d", stats.GetIndexCount());
		ImGui::Text("Frame Time: %.6fms", m_Ts * 1000.f);
		ImGui::Text("FPS: %d", int(1.f/m_Ts));
		ImGui::End(); //Stats
		
		//---------------------------Settings---------------------------
		ImGui::Begin("Settings");
		if (ImGui::Checkbox("VSync", &bVSync))
		{
			Application::Get().GetWindow().SetVSync(bVSync);
		}
		ImGui::Checkbox("Invert Colors", &m_InvertColor);
		ImGui::End(); //Settings

		//---------------------------Viewport---------------------------
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();

		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

		m_ViewportActive = ImGui::IsWindowHovered() && ImGui::IsWindowFocused();;
		
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
		m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

		uint64_t textureID = 0;
		textureID = (uint64_t)m_Framebuffer->GetColorAttachment((uint32_t)m_InvertColor);

		ImGui::Image((void*)textureID, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y}, { 0, 1 }, { 1, 0 });

		//---------------------------Gizmos---------------------------
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		SceneComponent* selectedComponent = m_SceneHierarchyPanel.GetSelectedComponent();

		if (selectedEntity && (m_GuizmoType != -1))
		{
			ImGuizmo::SetOrthographic(false); //TODO: Set to true when using Orthographic
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			//Camera

			//Runtime Camera
			//const auto& cameraComponent = cameraEntity.GetComponent<CameraComponent>();
			//const auto& camera = cameraComponent.Camera;
			//const glm::mat4& cameraProjection = camera.GetProjection();
			//glm::mat4 cameraViewMatrix = cameraComponent.GetViewMatrix();

			//Editor Camera
			const auto& editorCamera = m_ActiveScene->GetEditorCamera();
			const glm::mat4& cameraProjection = editorCamera.GetProjection();
			const glm::mat4& cameraViewMatrix = editorCamera.GetViewMatrix();

			Transform transform;
			//Entity transform
			if (selectedComponent)
			{
				transform = selectedComponent->GetWorldTransform();
			}
			else
			{
				transform = selectedEntity.GetWorldTransform();
			}

			glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

			//Snapping
			bool bSnap = Input::IsKeyPressed(Key::LeftShift);
			float snapVal = 0.1f;
			if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				snapVal = 10.f;

			float snapValues[3] = {snapVal, snapVal, snapVal};

			ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
								ImGuizmo::LOCAL, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				glm::vec3 deltaRotation;
				Math::DecomposeTransformMatrix(transformMatrix, transform.Translation, deltaRotation, transform.Scale3D);

				if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
					transform.Rotation = deltaRotation;

				if (selectedComponent)
				{
					selectedComponent->SetWorldTransform(transform);
				}
				else
				{
					selectedEntity.SetWorldTransform(transform);
				}
			}
		}

		ImGui::End(); //Viewport
		ImGui::PopStyleVar();

		m_SceneHierarchyPanel.OnImGuiRender();

		EndDocking();
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
			return false;

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
				else if (control)
					SaveScene();
				break;
		}

		//Gizmos
		if (!ImGuizmo::IsUsing())
		{
			switch (e.GetKeyCode())
			{
			case Key::Q:
				m_GuizmoType = -1;
				break;

			case Key::W:
				m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;

			case Key::E:
				m_GuizmoType = ImGuizmo::OPERATION::ROTATE;
				break;

			case Key::R:
				m_GuizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			}
		}

		return false;
	}

	void EditorLayer::NewScene()
	{
		m_ActiveScene = MakeRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);

		m_OpenedScene = "";
		Input::SetWindowTitle(m_WindowTitle + std::string(" - Untitled.eagle"));
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

			m_OpenedScene = filepath;
			Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScene.string());
		}
	}

	void EditorLayer::SaveScene()
	{
		if (m_OpenedScene == "")
		{
			std::string filepath = FileDialog::SaveFile("Eagle Scene (*.eagle)\0*.eagle\0");
			if (!filepath.empty())
			{
				EG_CORE_TRACE("Saving Scene at '{0}'", filepath);
				SceneSerializer serializer(m_ActiveScene);
				serializer.Serialize(filepath);

				m_OpenedScene = filepath;
				Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScene.string());
			}
			else
			{
				EG_CORE_ERROR("Couldn't save scene {0}", filepath);
			}
		}
		else
		{
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(m_OpenedScene.string());
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialog::SaveFile("Eagle Scene (*.eagle)\0*.eagle\0");
		if (!filepath.empty())
		{
			EG_CORE_TRACE("Saving Scene at '{0}'", filepath);
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filepath);

			m_OpenedScene = filepath;
			Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScene.string());
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
