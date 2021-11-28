#include "EditorLayer.h"

#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Math/Math.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Audio/AudioEngine.h"
#include "Eagle/Audio/Sound2D.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
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
		, m_SceneHierarchyPanel(*this)
		, m_EditorSerializer(this)
		, m_ContentBrowserPanel(*this)
		, m_Window(Application::Get().GetWindow())
	{}

	void EditorLayer::OnAttach()
	{
		ScriptEngine::LoadAppAssembly("Sandbox.dll");
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;

		m_EditorScene = MakeRef<Scene>();
		SetCurrentScene(m_EditorScene);
		m_WindowTitle = m_Window.GetWindowTitle();

		if (m_EditorSerializer.Deserialize("../Sandbox/Engine/EditorDefault.ini") == false)
		{
			m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
		}

		if (m_OpenedScenePath.empty() || !std::filesystem::exists(m_OpenedScenePath))
		{
			NewScene();
		}
		else
		{
			SceneSerializer ser(m_EditorScene);
			if (ser.Deserialize(m_OpenedScenePath))
			{
				m_EnableSkybox = m_EditorScene->IsSkyboxEnabled();
				if (m_EditorScene->m_Cubemap)
					m_CubemapFaces = m_EditorScene->m_Cubemap->GetTextures();
				UpdateEditorTitle(m_OpenedScenePath);
			}
		}
	
		SoundSettings soundSettings;
		soundSettings.Volume = 0.25f;
		m_PlaySound = Sound2D::Create("assets/audio/playsound.wav", soundSettings);
	}

	void EditorLayer::OnDetach()
	{
		m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
		Scene::SetCurrentScene(nullptr);
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		static bool bRequiresScriptsRebuild = false;

		EG_PROFILE_FUNCTION();
		m_Ts = ts;
		 
		if (Utils::WereScriptsRebuild() || bRequiresScriptsRebuild)
		{
			if (m_EditorState == EditorState::Edit)
			{
				bRequiresScriptsRebuild = false;
				ScriptEngine::LoadAppAssembly("Sandbox.dll");
			}
			else
				bRequiresScriptsRebuild = true;
		}

		if (m_NewViewportSize != m_CurrentViewportSize) //If size changed, resize framebuffer
		{
			m_CurrentViewportSize = m_NewViewportSize;
			m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		}

		{
			EG_PROFILE_SCOPE("EditorLayer::Draw Scene");
			if (!m_ViewportHidden)
			{
				if (m_EditorState == EditorState::Edit)
					m_EditorScene->OnUpdateEditor(ts);
				else if (m_EditorState == EditorState::Play)
					m_SimulationScene->OnUpdateRuntime(ts);
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
				int pixelData = m_CurrentScene->GetEntityIDAtCoords(mouseX, mouseY);
 				m_SceneHierarchyPanel.SetEntitySelected(pixelData);
			}
		}
	}

	void EditorLayer::OnEvent(Eagle::Event& e)
	{
		if (!m_ViewportHidden)
		{
			if (m_EditorState == EditorState::Edit)
				m_EditorScene->OnEventEditor(e);
			else if (m_EditorState == EditorState::Play)
				m_SimulationScene->OnEventRuntime(e);
		}
		m_SceneHierarchyPanel.OnEvent(e);
		m_ContentBrowserPanel.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_PROFILE_FUNCTION();

		BeginDocking();
		m_VSync = Application::Get().GetWindow().IsVSync();
		static uint64_t textureID = (uint64_t)m_CurrentScene->GetMainColorAttachment(0);

		//---------------------------Menu bar---------------------------
		{
			static bool bShowShaders = false;
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

				if (ImGui::BeginMenu("Debug"))
				{
					if (ImGui::BeginMenu("G-Buffer"))
					{
						static int selectedTexture = -1;
						int oldValue = selectedTexture;

						if (ImGui::RadioButton("Position", &selectedTexture, 0))
						{
							if (oldValue == selectedTexture)
							{
								selectedTexture = -1;
								textureID = (uint64_t)m_CurrentScene->GetMainColorAttachment(0);
							}
							else
								textureID = (uint64_t)m_CurrentScene->GetGBufferColorAttachment(selectedTexture);
						}
						if (ImGui::RadioButton("Normals", &selectedTexture, 1))
						{
							if (oldValue == selectedTexture)
							{
								selectedTexture = -1;
								textureID = (uint64_t)m_CurrentScene->GetMainColorAttachment(0);
							}
							else
								textureID = (uint64_t)m_CurrentScene->GetGBufferColorAttachment(selectedTexture);
						}
						if (ImGui::RadioButton("Albedo", &selectedTexture, 2))
						{
							if (oldValue == selectedTexture)
							{
								selectedTexture = -1;
								textureID = (uint64_t)m_CurrentScene->GetMainColorAttachment(0);
							}
							else
								textureID = (uint64_t)m_CurrentScene->GetGBufferColorAttachment(selectedTexture);
						}
						ImGui::EndMenu();
					}
					
					ImGui::Checkbox("Show Shaders", &bShowShaders);
					ImGui::EndMenu();
				}

				static bool bShowHelp = false;
				if (ImGui::MenuItem("Help"))
					bShowHelp = true;

				if (bShowHelp)
					ShowHelpWindow(&bShowHelp);
				ImGui::EndMenuBar();
			}
		
			if (bShowShaders)
			{
				const auto& shaders = ShaderLibrary::GetAllShaders();
				ImGui::Begin("Shaders", &bShowShaders);
				for (auto& it : shaders)
				{
					std::string filename = it.first.filename().u8string();
					ImGui::PushID(filename.c_str());
					ImGui::Text(filename.c_str());
					ImGui::SameLine();
					if (ImGui::Button("Reload"))
						it.second->Reload();
					ImGui::PopID();
				}
				ImGui::End();
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
				UI::BeginPropertyGrid("SkyboxSceneSettings");
				static bool bShowError = false;
				m_EnableSkybox = m_CurrentScene->IsSkyboxEnabled();
				UI::Property("Enable Skybox", m_EnableSkybox);

				if (m_EnableSkybox)
				{
					if (!CanRenderSkybox())
					{
						m_EnableSkybox = false;
						bShowError = true;
					}
				}
				m_CurrentScene->SetEnableSkybox(m_EnableSkybox);
				
				UI::DrawTextureSelection("Right", m_CubemapFaces[0], true);
				UI::DrawTextureSelection("Left", m_CubemapFaces[1], true);
				UI::DrawTextureSelection("Top", m_CubemapFaces[2], true);
				UI::DrawTextureSelection("Bottom", m_CubemapFaces[3], true);
				UI::DrawTextureSelection("Front", m_CubemapFaces[4], true);
				UI::DrawTextureSelection("Back", m_CubemapFaces[5], true);

				if (UI::Button("Create Skybox", "Create"))
				{
					bool canCreate = true;
					for (int i = 0; canCreate && (i < m_CubemapFaces.size()); ++i)
						canCreate = canCreate && m_CubemapFaces[i];

					if (canCreate)
						m_CurrentScene->m_Cubemap = Cubemap::Create(m_CubemapFaces);
					else
						bShowError = true;
				}

				if (bShowError)
					if (UI::ShowMessage("Error", "Can't use skybox! Select all required textures and press 'Create'!", UI::ButtonType::OK) == UI::ButtonType::OK)
						bShowError = false;

				ImGui::TreePop();
				UI::EndPropertyGrid();
			}

			float gamma = m_CurrentScene->GetSceneGamma();
			float exposure = m_CurrentScene->GetSceneExposure();
			ImGui::Separator();
			UI::BeginPropertyGrid("SceneGammaSettings");
			UI::PropertyDrag("Gamma", gamma, 0.1f, 0.0f, 10.f);
			UI::PropertyDrag("Exposure", exposure, 0.1f, 0.0f, 100.f);
			UI::EndPropertyGrid();

			m_CurrentScene->SetSceneGamma(gamma);
			m_CurrentScene->SetSceneExposure(exposure);

			ImGui::End();
			ImGui::PopID();
		}

		//---------------------------Settings---------------------------
		{
			ImGui::Begin("Settings");
			UI::BeginPropertyGrid("SettingsPanel");
			if (UI::Property("VSync", m_VSync))
			{
				Application::Get().GetWindow().SetVSync(m_VSync);
			}
			if (UI::Button("Reload shaders", "Reload"))
				ShaderLibrary::ReloadAllShader();
			UI::EndPropertyGrid();
			ImGui::End(); //Settings
		}

		//---------------------Editor Preferences-----------------------
		{
			constexpr uint64_t treeID = 95242191ull;
			glm::vec3 tempSnappingValues = m_SnappingValues;
			ImGui::Begin("Editor Preferences");

			constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx((void*)treeID, flags, "Snapping");
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("EditorPreferences");
				if (UI::InputFloat("Location", tempSnappingValues[0], 0.1f, 1.f))
				{
					if (tempSnappingValues[0] >= 0.f)
						m_SnappingValues[0] = tempSnappingValues[0];
				}
				if (UI::InputFloat("Rotation", tempSnappingValues[1], 1.f, 5.f))
				{
					if (tempSnappingValues[1] >= 0.f)
						m_SnappingValues[1] = tempSnappingValues[1];
				}
				if (UI::InputFloat("Scale", tempSnappingValues[2], 0.1f, 1.f))
				{
					if (tempSnappingValues[2] >= 0.f)
						m_SnappingValues[2] = tempSnappingValues[2];
				}
				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
			ImGui::Separator();

			ImGuiLayer::ShowStyleSelector("Style", &m_EditorStyleIdx);

			ImGui::End(); //Editor Preferences
		}

		//---------------------------Viewport---------------------------
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			m_ViewportHidden = !ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

			if (!m_ViewportHidden)
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();

				m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

				m_ViewportHovered = ImGui::IsWindowHovered();
				m_ViewportFocused = ImGui::IsWindowFocused();

				if (m_EditorState == EditorState::Edit)
				{
					if (ImGui::IsMouseReleased(1))
						m_EditorScene->bCanUpdateEditorCamera = false;
					else if (m_EditorScene->bCanUpdateEditorCamera || (m_ViewportHovered && ImGui::IsMouseClicked(1, true)))
						m_EditorScene->bCanUpdateEditorCamera = true;
				}

				ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
				m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

				ImGui::Image((void*)textureID, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y }, { 0, 1 }, { 1, 0 });
			}
		}

		//---------------------------Gizmos---------------------------
		{
			if (m_EditorState == EditorState::Edit)
				UpdateGuizmo();

			ImGui::End(); //Viewport
			ImGui::PopStyleVar();
		}

		//---------------------Simulate panel---------------------------
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.305f, 0.31f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.1505f, 0.151f, 0.5f));
			void* simulateTextureID = (void*)(uint64_t)(m_EditorState == EditorState::Edit ? Texture2D::PlayButtonTexture : Texture2D::StopButtonTexture)->GetRendererID();

			ImGui::Begin("##tool_bar", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			float size = ImGui::GetWindowHeight() - 4.0f;
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x / 2.0f) - (1.5f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)) - (size / 2.0f));
			if (ImGui::ImageButton(simulateTextureID, { size, size }, { 0, 1 }, { 1, 0 }))
			{
				if (m_EditorState == EditorState::Edit)
				{
					m_EditorState = EditorState::Play;
					m_SimulationScene = MakeRef<Scene>(m_EditorScene);
					SetCurrentScene(m_SimulationScene);
					m_SimulationScene->OnRuntimeStart();
					m_PlaySound->Play();
				}
				else if (m_EditorState != EditorState::Edit)
				{
					m_SimulationScene->OnRuntimeStop();
					m_EditorState = EditorState::Edit;
					m_SimulationScene.reset();
					SetCurrentScene(m_EditorScene);
				}
			}

			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
			ImGui::End();
		}

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
		if (m_EditorState == EditorState::Edit)
		{
			ComponentsNotificationSystem::ResetSystem();
			ScriptEngine::Reset();
			m_EditorScene = MakeRef<Scene>();
			SetCurrentScene(m_EditorScene);
			m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			m_EnableSkybox = false;
			m_OpenedScenePath = "";
			UpdateEditorTitle("Untitled.eagle");
		}
	}

	void EditorLayer::OpenScene()
	{
		if (m_EditorState == EditorState::Edit)
		{
			std::filesystem::path filepath = FileDialog::OpenFile(FileDialog::SCENE_FILTER);
			OpenScene(filepath);
		}
	}

	void EditorLayer::OpenScene(const std::filesystem::path& filepath)
	{
		if (m_EditorState == EditorState::Edit)
		{
			if (filepath.empty())
			{
				EG_CORE_ERROR("Failed to load scene. Path is null");
				return;
			}

			ComponentsNotificationSystem::ResetSystem();
			ScriptEngine::Reset();
			m_EditorScene = MakeRef<Scene>();
			SetCurrentScene(m_EditorScene);
			m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);

			SceneSerializer serializer(m_EditorScene);
			serializer.Deserialize(filepath);
			m_EnableSkybox = m_EditorScene->IsSkyboxEnabled();
			if (m_EditorScene->m_Cubemap)
				m_CubemapFaces = m_EditorScene->m_Cubemap->GetTextures();

			m_OpenedScenePath = filepath;
			UpdateEditorTitle(m_OpenedScenePath);
		}
	}

	void EditorLayer::SaveScene()
	{
		if (m_EditorState == EditorState::Edit)
		{
			if (m_OpenedScenePath == "")
			{
				std::filesystem::path filepath = FileDialog::SaveFile(FileDialog::SCENE_FILTER);
				if (!filepath.empty())
				{
					SceneSerializer serializer(m_EditorScene);
					serializer.Serialize(filepath);

					m_OpenedScenePath = filepath;
					UpdateEditorTitle(m_OpenedScenePath);
				}
				else
				{
					EG_CORE_ERROR("Couldn't save scene {0}", filepath);
				}
			}
			else
			{
				SceneSerializer serializer(m_EditorScene);
				serializer.Serialize(m_OpenedScenePath);
			}
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		if (m_EditorState == EditorState::Edit)
		{
			std::filesystem::path filepath = FileDialog::SaveFile(FileDialog::SCENE_FILTER);
			if (!filepath.empty())
			{
				SceneSerializer serializer(m_EditorScene);
				serializer.Serialize(filepath);

				m_OpenedScenePath = filepath; UpdateEditorTitle(m_OpenedScenePath);
			}
			else
			{
				EG_CORE_ERROR("Couldn't save scene {0}", filepath);
			}
		}
	}

	void EditorLayer::UpdateEditorTitle(const std::filesystem::path& scenePath)
	{
		std::string displayName = scenePath.u8string();
		size_t contentPos = displayName.find("Content");
		if (contentPos != std::string::npos)
		{
			displayName = displayName.substr(contentPos);
			m_Window.SetWindowTitle(m_WindowTitle + std::string(" - ") + displayName);
		}
		else
		{
			m_Window.SetWindowTitle(m_WindowTitle + std::string(" - ") + scenePath.u8string());
		}

	}

	bool EditorLayer::CanRenderSkybox() const
	{
		return m_CurrentScene->m_Cubemap.operator bool();
	}

	void EditorLayer::OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, bool bWindowMaximized)
	{
		Window& window = Application::Get().GetWindow();
		window.SetVSync(m_VSync);
		ImGuiLayer::SelectStyle(m_EditorStyleIdx);
		if ((int)windowSize.x > 0 && (int)windowSize.y > 0)
		{
			window.SetWindowSize((int)windowSize[0], (int)windowSize[1]);
		}
		if (windowPos.x >= 0 && windowPos.y >= 0)
		{
			window.SetWindowPos((int)windowPos.x, (int)windowPos.y);
		}
		window.SetWindowMaximized(bWindowMaximized);
	}

	void EditorLayer::SetCurrentScene(const Ref<Scene>& scene)
	{
		m_CurrentScene = scene;
		Scene::SetCurrentScene(m_CurrentScene);
		m_SceneHierarchyPanel.SetContext(m_CurrentScene);
		AudioEngine::DeletePlayingSingleshotSound();
	}

	void EditorLayer::UpdateGuizmo()
	{
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		SceneComponent* selectedComponent = m_SceneHierarchyPanel.GetSelectedComponent();

		if (selectedEntity && (m_GuizmoType != -1))
		{
			ImGuizmo::SetOrthographic(false); //TODO: Set to true when using Orthographic
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			//Camera
			const auto& editorCamera = m_EditorScene->GetEditorCamera();
			const auto runtimeCamera = m_CurrentScene->GetRuntimeCamera();
			const bool bEditing = m_EditorState == EditorState::Edit;
			const glm::mat4& cameraProjection = bEditing ? editorCamera.GetProjection() : runtimeCamera->Camera.GetProjection();
			const glm::mat4& cameraViewMatrix = bEditing ? editorCamera.GetViewMatrix() : runtimeCamera->GetViewMatrix();

			Transform transform;
			//Entity transform
			if (selectedComponent)
				transform = selectedComponent->GetWorldTransform();
			else
				transform = selectedEntity.GetWorldTransform();

			glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

			//Snapping
			bool bSnap = Input::IsKeyPressed(Key::LeftShift);

			int snappingIndex = 0;
			if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				snappingIndex = 1;
			else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
				snappingIndex = 2;

			float snapValues[3] = { m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex] };

			ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
				ImGuizmo::WORLD, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				static glm::vec3 notUsed1; glm::vec4 notUsed2;
				glm::quat newRotation;

				glm::decompose(transformMatrix, transform.Scale3D, newRotation, transform.Location, notUsed1, notUsed2);

				if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				{
					transform.Rotation = newRotation;
				}

				if (selectedComponent)
					selectedComponent->SetWorldTransform(transform);
				else
					selectedEntity.SetWorldTransform(transform);
			}
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

		ImGui::PushID("HelpWindow");
		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		if (ImGui::TreeNodeEx("Basics", flags, "Basics"))
		{
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
			ImGui::BulletText("Supported texture format: 4 channel PNG, 3 channel JPG");
			ImGui::BulletText("Supported 3D-Model formats: fbx, blend, 3ds, obj, smd, vta, stl.\nNote that a single file can contain multiple meshes. If a model containes information about textures, Engine will try to load them as well.");
			ImGui::TreePop();
		}
		ImGui::Separator();

		ImGui::SetWindowFontScale(1.5f);
		if (ImGui::TreeNodeEx("Version 0.4", flags, "Version 0.4"))
		{
			ImGui::Separator();
			ImGui::Separator();
			ImGui::SetWindowFontScale(3.f);
			ImGui::Text("New");
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Separator();
			ImGui::Separator();
			ImGui::Text("Rendering");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Deferred Rendering was implemented. Deferred Rendering significantly improves performance in large scenes with lots of light sources.");
			ImGui::BulletText("Directional Light now casts shadows!");
			ImGui::BulletText("Point Lights now cast shadows!");
			ImGui::BulletText("Spot Lights now cast shadows!");
			ImGui::BulletText("Normal Mapping! Added support for normal textures.");
			ImGui::BulletText("Added Gamma correction.");
			ImGui::BulletText("Added HDR");
			ImGui::BulletText("None Textures. Now you can set any texture to be None.");
			ImGui::BulletText("Added Diffuse Tint color to Material. Now if you don't want to use any textures, you can set diffuse texture to None and adjust objects color by using 'Tint Color' parameter.");
			ImGui::BulletText("Added support for Geometry Shaders.");
			ImGui::BulletText("Added Uniform buffers.");
			ImGui::BulletText("Added Cubemaps.");
			ImGui::BulletText("Added sRGB support.");
			ImGui::BulletText("Implemented Blinn-Phong's model.");
			ImGui::BulletText("Enabled MSAA x4.");
			ImGui::BulletText("Enabled back-face culling.");

			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Content Browser");
			ImGui::SetWindowFontScale(1.2f);
			ImGui::Separator();
			ImGui::Text("Added Content Browser that allows you to navigate through asset files and open some of them.");
			ImGui::BulletText("Searching. Content browser allows you to search for files.");
			ImGui::BulletText("Navigation. To navigate, double-click image or single-click file's/folder's name.\nIf the name of a file doesn't fit inside a button, you can hover it to see its full name.");
			ImGui::BulletText("Navigation History. Underneath the search panel you can see the navigation history.\nYou can navigate back by pressing history buttons or clicking back/forward buttons. Also you can use additional mouse buttons to navigate back/forward.");
			ImGui::BulletText("Right-click popup. You can right-click anything in content browser and press 'Show in Explorer' to show it in explorer.\nOr you can right-click any file (not directory) and press 'Show in Folder View' to show it in Content Browser.\nIt's useful if you found a file using search and want to navigate to its location inside Content Browser.");
			ImGui::BulletText("Content Browser can open scenes.");
			ImGui::BulletText("Drag&Drop. You can Drag&Drop Textures/Meshes from Content Browser to Texture/Mesh Slots.");
			ImGui::BulletText("Open Textures (Texture Viewer). Texture Viewer shows some texture details:\nName; Size; If it's sRGB texture or not. Also it allows you to convent texture to sRGB or to non-sRGB format.");
			ImGui::BulletText("Content browser supports cyrillic.");

			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("UI");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Styles! Now you can set style of the editor in 'Editor Preferences' panel.");
			ImGui::BulletText("Added 'Scene Settings' panel. You can use it to set skybox and adjust gamma and exposure.");
			ImGui::BulletText("Shaders reloading. Added 'Reload shaders' button to 'Settings' panel.");
			ImGui::BulletText("Added Debug Menu panel. It allows you to:\nVisualize G-Buffer. Namely: position, normals, albedo.\nSee all shaders where you can reload any of them.");
			
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Other");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("JPG textures can now be imported.");
			ImGui::BulletText("Added 'Shadows' scene.");
			ImGui::BulletText("Added 'HDRScene' scene.");
			ImGui::BulletText("Engine now has logo and icon. Made by Verual");

			ImGui::Separator();
			ImGui::Separator();
			ImGui::SetWindowFontScale(3.f);
			ImGui::Text("Improvements");
			ImGui::Separator();

			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Rendering");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Improved Materials.");
			ImGui::BulletText("Improved Frame buffers");
			ImGui::BulletText("Improved 2D Rendering.");
			ImGui::BulletText("Improved 3D Rendering by implementing batching. Batching improved performance by 100% (~300 fps vs. ~600 fps on my PC).\nBut! Since shadows were added, performance dropped to ~160 fps on my PC. (i5-9400f, 16GB DDR4, GTX 1660Ti)");
			ImGui::BulletText("Improved Rendering by sorting objects based on distance from camera. (The closest objects are rendered first)");

			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("UI");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Texture Selection. Now you can click on texture in 'Texture Selection' dropmenu.\nIn previous versions you had to click on a texture's name in order to select it.");

			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Other");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Improved Scene Serializer.");
			ImGui::BulletText("Improved Help Window.");
			ImGui::BulletText("Improved Editor camera's behavior.");
			ImGui::BulletText("Removed 'Invert Color' visualization.");

			ImGui::Separator();
			ImGui::Separator();
			ImGui::SetWindowFontScale(3.f);
			ImGui::Text("Fixes");
			ImGui::Separator();
			ImGui::Separator();

			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Fixed wrong normals of objects.");
			ImGui::BulletText("Fixed bug when positions of children (attached entities and added components) were not updating");
			ImGui::BulletText("Fixed Lights not using Ambient color.");
			ImGui::BulletText("Fixed Vertex Buffer layout. Now it supports Mat3 & Mat4.");
			ImGui::BulletText("Fixed importing multiple meshes as a single file.");

			ImGui::Separator();
			ImGui::Separator();
			ImGui::SetWindowFontScale(3.f);
			ImGui::Text("Minor");
			ImGui::Separator();
			ImGui::Separator();

			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Updated '3DScene' scene.");
			ImGui::BulletText("Disabled Alpha-Blending. It was not working anyway because of enabled Depth-Test.");
			ImGui::BulletText("Removed unused shaders.");
			ImGui::BulletText("Renamed some shaders.");
			ImGui::BulletText("Changed default Point Lights distance value from 100 to 1.");
			ImGui::BulletText("Alpha in shaders is always 1.0 (again, changes nothing)");

			ImGui::Separator();
			ImGui::Separator();
			ImGui::SetWindowFontScale(3.f);
			ImGui::Text("Keep in mind");
			ImGui::Separator();
			ImGui::Separator();

			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Objects that are being rendered use Specular as Reflection map for reflecting Skybox (Temporary).");
			ImGui::BulletText("Content Browser is not updating if it's not hovered.\nMeaning that if new files were created, they won't appear in content browser unless you hover it.");
			ImGui::BulletText("Creating more then 10000 entities can crash editor. This limit can be changed in Scene.cpp by changing value of a variable 'maxEntities'.\nHopefully, this problem will be fixed in the next versions of the Engine.");

			ImGui::Separator();
			ImGui::TreePop();
		}

		ImGui::SetWindowFontScale(1.5f);
		if (ImGui::TreeNodeEx("Version 0.3", flags, "Version 0.3"))
		{
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("New");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Added Static Mesh Component, i.e. you can now import and use 3D models in a properties pane.\nSupported 3D-Model formats: fbx, blend, 3ds, obj, smd, vta, stl.\nIf an importing model contains information about textures, Engine will try to load them as well. Note that a single file can contain multiple meshes, each of them will be loaded as separate Static Mesh;");
			ImGui::BulletText("Added SubTexture (Atlas) support to SpriteComponent. To use a subtexture from an Atlas in Sprite Component, check 'Is Subtexture' in Sprite Components propeties and select an Atlas Texture.\nSet size of a single sprite in atlas, set sprite's coords (starting from bottom left in 0;0). In case some sprites have different sizes, change 'Sprite Size Coef'.\nOpen '3DScene' scene for an example.");
			ImGui::BulletText("Added Alpha Support in Shaders (Renderer still doesn't support transparency)");
			ImGui::BulletText("Added 'TilingFactor' to Material and to Shader. Also exposed that value to UI");
			ImGui::BulletText("Added StaticMeshShader for Mesh Rendering");
			ImGui::BulletText("Added new scene '3DScene'");
			ImGui::BulletText("Added Renderer3D Stats");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Update");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Now Engine tries to save a relative path to using assets");
			ImGui::BulletText("Bringed back GuizmoType::None. It means that you can press Q to hide Guizmo");
			ImGui::BulletText("Updated Renderer class to render 3D models");
			ImGui::BulletText("DEL now works if SceneHierarchy or Viewport are hovered");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Minor");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Added 'Cast' Macro that allows casting Ref's");
			ImGui::BulletText("Added Static Mesh Library");
			ImGui::BulletText("Minor Updates in Renderer 2D");
			ImGui::Separator();
			ImGui::TreePop();
		}
		
		ImGui::SetWindowFontScale(1.5f);
		if (ImGui::TreeNodeEx("Version 0.2.2", flags, "Version 0.2.2"))
		{
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("New");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Added Help Window");
			ImGui::BulletText("Added Editor Preferences");
			ImGui::BulletText("Added Editor Serializer that saves Editor Preferences when Eagle-Editor is shutting down.\nValues that are being saved:\n\t1) Guizmo Type\n\t2) Snapping Values\n\t3) VSync state\n\t4) 'Invert Colors' state\n\t5) Window's size\n\t6) Window's position");
			ImGui::Separator();
			ImGui::TreePop();
		}

		ImGui::SetWindowFontScale(1.5f);
		if (ImGui::TreeNodeEx("Version 0.2.1", flags, "Version 0.2.1"))
		{
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("New");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Now entities can be deleted by pressing DEL key.");
			ImGui::BulletText("Spot Light. Scene now supports only 4 Spot Light Sources.");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Fixes");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Crash could occur when deleting an entity");
			ImGui::BulletText("SceneComponent's WorldTransform was setting to 0 when adding a new SceneComponent");
			ImGui::BulletText("Not all children were detaching from a 'pending to die' entity");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Minor");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Added Logging when Saving a Scene;");
			ImGui::Separator();
			ImGui::TreePop();
		}

		ImGui::SetWindowFontScale(1.5f);
		if (ImGui::TreeNodeEx("Version 0.2", flags, "Version 0.2"))
		{
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("New");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Added simple material");
			ImGui::BulletText("Added support for Diffuse and Specular textures.");
			ImGui::BulletText("Added Texture Library");
			ImGui::BulletText("Added Texture Selection in Components Window");
			ImGui::BulletText("Added Sprite Component to which you can assign a texture to it from a dropdown menu.");
			ImGui::BulletText("Updated ImGui Library");
			ImGui::BulletText("Improved Scene Serializer");
			ImGui::BulletText("Added Point Light Component. Works like a light bulb. It emits light in every direction around it.\nScene supports only 4 Point Light Sources.");
			ImGui::BulletText("Added Spot Light Component. Works like a street light. It emits light in a certain direction around it.\nScene supports only 1 Spot Light Sources.");
			ImGui::BulletText("Added Directional Light Component. It lights up a whole scene like a sun. Change its rotation to set lighting angle.\nScene supports only 1 Directional Light Source.");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("Fixes");
			ImGui::Separator();
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("Fixed Guizmos. Now Guizmos rotates along with object");
			ImGui::BulletText("Children now get attached to parent of entity that is about to be deleted");
			ImGui::BulletText("Minor UI Improvements");
			ImGui::BulletText("Fixed wrong viewport size when viewport tab is visible");
			ImGui::BulletText("Fixed Deserializer bug when it was not loading scene properly via hash-collisions");
			ImGui::Separator();
			ImGui::TreePop();
		}
	
		ImGui::Separator();
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("By Shikhali Shikhaliev.");
		ImGui::Text("Eagle Engine is licensed under the Apache-2.0 License, see LICENSE for more information.");
		ImGui::PopID();
		ImGui::End();
	}
}
