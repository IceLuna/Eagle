#include "EditorLayer.h"

#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Math/Math.h"
#include "Eagle/UI/UI.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo.h>

namespace Eagle
{
	static void BeginDocking();
	static void EndDocking();
	static void ShowHelpWindow(bool* p_open = nullptr);

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
		, m_EditorSerializer(this)
		, m_ContentBrowserPanel(*this)
	{}

	void EditorLayer::OnAttach()
	{
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;

		FramebufferSpecification fbSpecs;
		fbSpecs.Width = (uint32_t)m_CurrentViewportSize.x;
		fbSpecs.Height = (uint32_t)m_CurrentViewportSize.y;
		fbSpecs.Attachments = {{FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RED_INTEGER}, {FramebufferTextureFormat::Depth}};

		m_Framebuffer = Framebuffer::Create(fbSpecs);
		m_ActiveScene = MakeRef<Scene>();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_WindowTitle = Input::GetWindowTitle();

		if (m_EditorSerializer.Deserialize("Engine/EditorDefault.ini") == false)
		{
			m_EditorSerializer.Serialize("Engine/EditorDefault.ini");
		}

		if (m_OpenedScenePath.empty() || !std::filesystem::exists(m_OpenedScenePath))
		{
			NewScene();
		}
		else
		{
			SceneSerializer ser(m_ActiveScene);
			if (ser.Deserialize(m_OpenedScenePath.string()))
			{
				m_EnableSkybox = m_ActiveScene->IsSkyboxEnabled();
				if (m_ActiveScene->cubemap)
					m_CubemapFaces = m_ActiveScene->cubemap->GetTextures();
				Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScenePath.string());
			}
		}
	}

	void EditorLayer::OnDetach()
	{
		m_EditorSerializer.Serialize("Engine/EditorDefault.ini");
	}

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

			Renderer::ResetStats();
			Renderer2D::ResetStats();
			{
				EG_PROFILE_SCOPE("EditorLayer::Draw Scene");
				m_ActiveScene->OnUpdateEditor(ts);
			}
		}

		//Entity Selection
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		bool bUsingImGuizmo = selectedEntity && (ImGuizmo::IsUsing() || ImGuizmo::IsOver());
		if (m_ViewportHovered && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
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
		m_SceneHierarchyPanel.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_PROFILE_FUNCTION();

		BeginDocking();
		m_VSync = Application::Get().GetWindow().IsVSync();


		//---------------------------Menu bar---------------------------
		{
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

				static bool bShowHelp = false;
				if (ImGui::BeginMenu("Help"))
				{
					bShowHelp = Input::IsMouseButtonPressed(Mouse::ButtonLeft);
					ImGui::EndMenu();
				}
				if (bShowHelp)
					ShowHelpWindow(&bShowHelp);
				ImGui::EndMenuBar();
			}
		}

		//-----------------------------Stats----------------------------
		{
			ImGui::PushID("RendererStats");
			ImGui::Begin("Stats");

			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

			bool renderer3DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer3D", flags, "Renderer3D Stats");
			if (renderer3DTreeOpened)
			{
				auto stats = Renderer::GetStats();

				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Vertices: %d", stats.Vertices);
				ImGui::Text("Indices: %d", stats.Indeces);

				ImGui::TreePop();
			}

			bool renderer2DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer2D", flags, "Renderer2D Stats");
			if (renderer2DTreeOpened)
			{
				auto stats = Renderer2D::GetStats();
				
				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Quads: %d", stats.QuadCount);
				ImGui::Text("Vertices: %d", stats.GetVertexCount());
				ImGui::Text("Indices: %d", stats.GetIndexCount());

				ImGui::TreePop();
			}

			ImGui::Text("Frame Time: %.6fms", m_Ts * 1000.f);
			ImGui::Text("FPS: %d", int(1.f / m_Ts));
			ImGui::End(); //Stats

			ImGui::PopID();
		}
		
		//------------------------Scene Settings------------------------
		{
			ImGui::PushID("SceneSettings");
			ImGui::Begin("Scene Settings");
			constexpr uint64_t treeID = 95292191ull;

			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx((void*)treeID, flags, "Skybox");
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				static bool bShowError = false;
				ImGui::Checkbox("Enable Skybox", &m_EnableSkybox);

				if (m_EnableSkybox)
				{
					if (!CanRenderSkybox())
					{
						m_EnableSkybox = false;
						bShowError = true;
					}
				}
				m_ActiveScene->SetEnableSkybox(m_EnableSkybox);
				
				UI::DrawTextureSelection(m_CubemapFaces[0], "Right");
				UI::DrawTextureSelection(m_CubemapFaces[1], "Left");
				UI::DrawTextureSelection(m_CubemapFaces[2], "Top");
				UI::DrawTextureSelection(m_CubemapFaces[3], "Bottom");
				UI::DrawTextureSelection(m_CubemapFaces[4], "Front");
				UI::DrawTextureSelection(m_CubemapFaces[5], "Back");

				if (ImGui::Button("Create"))
				{
					bool canCreate = true;
					for (int i = 0; canCreate && (i < m_CubemapFaces.size()); ++i)
						canCreate = canCreate && m_CubemapFaces[i];

					if (canCreate)
						m_ActiveScene->cubemap = Cubemap::Create(m_CubemapFaces);
					else
						bShowError = true;
				}

				if (bShowError)
					if (UI::ShowMessage("Error", "Can't use skybox! Select all required textures and press 'Create'!", UI::Button::OK) == UI::Button::OK)
						bShowError = false;

				ImGui::TreePop();
			}

			ImGui::End();
			ImGui::PopID();
		}

		//---------------------------Settings---------------------------
		{
			ImGui::Begin("Settings");
			if (ImGui::Checkbox("VSync", &m_VSync))
			{
				Application::Get().GetWindow().SetVSync(m_VSync);
			}
			ImGui::Checkbox("Invert Colors", &m_InvertColors);
			ImGui::End(); //Settings
		}

		//---------------------Editor Preferences-----------------------
		{
			glm::vec3 tempSnappingValues = m_SnappingValues;
			ImGui::Begin("Editor Preferences");
			ImGui::Text("Snapping:");
			if (ImGui::InputFloat("Translation", &tempSnappingValues[0], 0.1f, 1.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (tempSnappingValues[0] >= 0.f)
					m_SnappingValues[0] = tempSnappingValues[0];
			}
			if (ImGui::InputFloat("Rotation", &tempSnappingValues[1], 1.f, 5.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (tempSnappingValues[1] >= 0.f)
					m_SnappingValues[1] = tempSnappingValues[1];
			}
			if (ImGui::InputFloat("Scale", &tempSnappingValues[2], 0.1f, 1.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (tempSnappingValues[2] >= 0.f)
					m_SnappingValues[2] = tempSnappingValues[2];
			}
			ImGui::Separator();
			ImGui::End(); //Editor Preferences
		}

		//---------------------------Viewport---------------------------
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();

		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

		m_ViewportHovered = ImGui::IsWindowHovered();
		m_ViewportFocused = ImGui::IsWindowFocused();
		
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
		m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

		uint64_t textureID = 0;
		textureID = (uint64_t)m_Framebuffer->GetColorAttachment((uint32_t)m_InvertColors);

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

			float snapValues[3] = { m_SnappingValues[m_GuizmoType], m_SnappingValues[m_GuizmoType], m_SnappingValues[m_GuizmoType] };

			ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
								ImGuizmo::WORLD, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

			if (m_ViewportHovered && ImGuizmo::IsUsing())
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
		m_ContentBrowserPanel.OnImGuiRender();

		EndDocking();

		//ImGui::ShowDemoWindow();
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
		if (m_ViewportHovered && !ImGuizmo::IsUsing())
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

		m_OpenedScenePath = "";
		Input::SetWindowTitle(m_WindowTitle + std::string(" - Untitled.eagle"));
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialog::OpenFile(FileDialog::SCENE_FILTER);
		OpenScene(filepath);
	}

	void EditorLayer::OpenScene(const std::string& filepath)
	{
		if (filepath.empty())
		{
			EG_CORE_ERROR("Failed to load scene: {0}", filepath);
			return;
		}

		m_ActiveScene = MakeRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);

		SceneSerializer serializer(m_ActiveScene);
		serializer.Deserialize(filepath);
		m_EnableSkybox = m_ActiveScene->IsSkyboxEnabled();
		if (m_ActiveScene->cubemap)
			m_CubemapFaces = m_ActiveScene->cubemap->GetTextures();

		m_OpenedScenePath = filepath;
		Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScenePath.string());
	}

	void EditorLayer::SaveScene()
	{
		if (m_OpenedScenePath == "")
		{
			std::string filepath = FileDialog::SaveFile(FileDialog::SCENE_FILTER);
			if (!filepath.empty())
			{
				SceneSerializer serializer(m_ActiveScene);
				serializer.Serialize(filepath);

				m_OpenedScenePath = filepath;
				Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScenePath.string());
			}
			else
			{
				EG_CORE_ERROR("Couldn't save scene {0}", filepath);
			}
		}
		else
		{
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(m_OpenedScenePath.string());
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialog::SaveFile(FileDialog::SCENE_FILTER);
		if (!filepath.empty())
		{
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filepath);

			m_OpenedScenePath = filepath;
			Input::SetWindowTitle(m_WindowTitle + std::string(" - ") + m_OpenedScenePath.string());
		}
		else
		{
			EG_CORE_ERROR("Couldn't save scene {0}", filepath);
		}
	}

	bool EditorLayer::CanRenderSkybox() const
	{
		return m_ActiveScene->cubemap.operator bool();
	}

	void EditorLayer::OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos)
	{
		Window& window = Application::Get().GetWindow();
		window.SetVSync(m_VSync);
		if ((int)windowSize.x > 0 && (int)windowSize.y > 0)
		{
			window.SetWindowSize((int)windowSize[0], (int)windowSize[1]);
		}
		if (windowPos.x >= 0 && windowPos.y >= 0)
		{
			window.SetWindowPos((int)windowPos.x, (int)windowPos.y);
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

	static void ShowHelpWindow(bool* p_open)
	{
		ImGui::Begin("Help", p_open);
		ImGui::SetWindowFontScale(2.f);
		ImGui::Text("Eagle Engine v%s", EG_VERSION);
		ImGui::SetWindowFontScale(1.2f);
		ImGui::Separator();
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("How to...");
		ImGui::SetWindowFontScale(1.2f);
		ImGui::BulletText("Left Click an object in the scene to select it. Press W/E/R/Q to enable Location/Rotation/Scale/Hidden manipulation mode.");
		ImGui::BulletText("Right Click on empty space in 'Scene Hierarchy' panel to create an entity.");
		ImGui::BulletText("To add components to an entity, select it and click 'Add' button in 'Properties' panel.");
		ImGui::BulletText("Hold LShift to enable snapping while manipulating object's transform (Snap values can be changed in 'Editor Preferences' panel).");
		ImGui::BulletText("To see Renderer Stats, open 'Stats' panel. You can change some Renderer Settings in 'Settings' panel.");
		ImGui::BulletText("Hold RMB on the scene to move camera. In that state, you can press W/A/S/D or Q/E to change camera's position.\n"
		"Also you can use mouse wheel to adjust camera's speed.");
		ImGui::BulletText("To delete an entity, right click it in the 'Scene Hierarchy' panel and press 'Delete Entity'. Or just press DEL while entity is selected.");
		ImGui::BulletText("To attach one entity to another, drag & drop it on top of another entity.");
		ImGui::BulletText("To detach one entity from another, drag & drop it on top of 'Scene Hierarchy' text.");
		ImGui::Separator();
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("Features...");
		ImGui::SetWindowFontScale(1.2f);
		ImGui::BulletText("Engine support only 2 texture types for now: Diffuse and Specular.");
		ImGui::BulletText("Supported texture format: 4 channel PNG, 3 channel JPG");
		ImGui::BulletText("Supported 3D-Model formats: fbx, blend, 3ds, obj, smd, vta, stl.\nNote that a single file can contain multiple meshes. If a model containes information about textures, Engine will try to load them as well.");
		ImGui::BulletText("Sprite Component to which you can assign a texture to it from a dropdown menu.");
		ImGui::BulletText("To use a subtexture from an Atlas in Sprite Component, check 'Is Subtexture' in Sprite Components propeties and select an Atlas Texture.\nSet size of a single sprite in atlas, set sprite's coords (starting from bottom left in 0;0). In case some sprites have different sizes, change 'Sprite Size Coef'.\nOpen '3DScene' scene for an example.");
		ImGui::BulletText("StaticMesh Component to which you can assign a 3D-model to it from a dropdown menu.");
		ImGui::BulletText("Point Light Component. Works like a light bulb. It emits light in every direction around it.\nScene supports only 4 Point Light Sources.");
		ImGui::BulletText("Spot Light Component. Works like a street light. It emits light in a certain direction around it.\nScene supports only 4 Spot Light Sources.");
		ImGui::BulletText("Directional Light Component. It lights up a whole scene like a sun. Change its rotation to set lighting angle.\nScene supports only 1 Directional Light Source.");
		ImGui::Separator();
		ImGui::Text("By Shikhali Shikhaliev.");
		ImGui::Text("Eagle Engine is licensed under the Apache-2.0 License, see LICENSE for more information.");
		ImGui::End();
	}
}
