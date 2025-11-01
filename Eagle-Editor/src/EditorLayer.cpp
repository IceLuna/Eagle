#include "EditorLayer.h"
#include "ProjectLayer.h"
#include "EditorResources.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Asset/AssetImporter.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Debug/CPUTimings.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo.h>
#include <implot.h>
#include <magic_enum.hpp>

namespace Eagle
{
	static const char* s_SkyHelpMsg = "Sky is used just for background! It doesn't actually light the scene at the moment!\nIf this is checked, IBL will still light the scene if it's set. The only thing that changes is background";
	static const char* s_EnableVolumetricLightsHelpMsg = "Note that this just notifies the engine that volumetric lights can be used! To use volumetric lights, you'll need to check `Is Volumetric` of a particular light";
	static const char* s_StutterlessHelpMsg = "If checked, Point/Spot/Dir lights info will be dynamically sent to shaders meaning it won't recompile and won't trigger recompilation. "
		"But since they'll become dynamic, the compiler won't be able to optimize some shader code making it run slower. "
		"So if you don't care much about the performance and want to avoid stutters when adding/removing lights, use this option. "
		"If unchecked, adding/removing lights MIGHT trigger some shaders recompilation since the light data is getting injected right into the shader source code which then needs to be recompiled. "
		"But it's not that bad because shaders are being cached. So if the engine sees the same light data again, there'll be no stutters since shaders won't be recompiled, they'll be just taken from the cache";
	static const char* s_MaxShadowDistHelpMsg = "Beyond this distance from camera, shadows won't be rendered. Note this setting applies only to the editor camera! You'll need to apply this value to CameraComponent if you want to see it in the simulation";
	static const char* s_CascadesSplitAlphaHelpMsg = "It's used to determine how to split cascades for directional light shadows. Note this setting applies only to the editor camera! You'll need to apply this value to CameraComponent if you want to see it in the simulation";
	static const char* s_CascadesSmoothTransitionAlphaHelpMsg = "The blend amount between cascades of directional light shadows (if smooth transition is enabled). Try to keep it as low as possible. Note this setting applies only to the editor camera! You'll need to apply this value to CameraComponent if you want to see it in the simulation";
	static const char* s_SkyboxEnableHelpMsg = "Affects Sky and IBL";
	static const char* s_TransparencyLayersHelpMsg = "More layers - better quality. But be careful when increasing this value since it requires a lot of memory. "
		"Memory consumption: `width * height * layers * 12` bytes";
	static const char* s_PhysicsDebugTypeHelpMsg = "When `Live` is selected, the data will be sent directly to PhysX Visual Debugger at runtime. Otherwise, it'll be saved to a file which can be opened later. "
		"The file is saved into `Saved` folder inside your project";

	static std::mutex s_DeferredCallsMutex;
	
	static glm::vec3 notUsed1;
	static glm::vec4 notUsed2;
	static const EditorLayer* s_EditorLayer = nullptr;

	static void ShowHelpWindow(bool* p_open = nullptr);

	static void DisplayTiming(const GPUTimingData& data, size_t i = 0)
	{
		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		constexpr float spacing = 10.f;
		
		const std::string timingStr = std::to_string(data.Timing);
		if (data.Children.empty())
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing * float(i));
			UI::Text(data.Name, timingStr.c_str());
			return;
		}

		// Calculate `collapser arrow width + Spacing` offset
		// in order to move a tree to the left so that children names are all aligned vertically
		float treeOffsetX = 0.f;
		if (i != 0)
		{
			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;
			const bool display_frame = (treeFlags & ImGuiTreeNodeFlags_Framed) != 0;
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const ImVec2 padding = (display_frame || (treeFlags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));
			treeOffsetX = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);
		}

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing * float(i) - treeOffsetX);

		UI::UpdateIDBuffer(data.Name);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		bool entityTreeOpened = ImGui::TreeNodeEx(data.Name.data(), treeFlags, data.Name.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::Text(timingStr.data());
		ImGui::PopItemWidth();
		ImGui::NextColumn();

		if (entityTreeOpened)
		{
			for (auto& child : data.Children)
				DisplayTiming(child, i + 1);

			ImGui::TreePop();
		}
	};

	static void DisplayTiming(const CPUTiming::Data& data, size_t i = 0)
	{
		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		constexpr float spacing = 10.f;
		
		const std::string timingStr = std::to_string(data.Timing);
		if (data.Children.empty())
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing * float(i));
			UI::Text(data.Name, timingStr.c_str());
			return;
		}

		// Calculate `collapser arrow width + Spacing` offset
		// in order to move a tree to the left so that children names are all aligned vertically
		float treeOffsetX = 0.f;
		{
			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;
			const bool display_frame = (treeFlags & ImGuiTreeNodeFlags_Framed) != 0;
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const ImVec2 padding = (display_frame || (treeFlags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));
			treeOffsetX = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);
		}
		if (i == 0)
			treeOffsetX *= 0.5f;

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing * float(i) - treeOffsetX);

		UI::UpdateIDBuffer(data.Name);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
		bool entityTreeOpened = ImGui::TreeNodeEx(data.Name.data(), treeFlags, data.Name.data());
		ImGui::NextColumn();
		ImGui::PushItemWidth(-1);
		ImGui::Text(timingStr.data());
		ImGui::PopItemWidth();
		ImGui::NextColumn();

		if (entityTreeOpened)
		{
			for (auto& child : data.Children)
				DisplayTiming(child, i + 1);

			ImGui::TreePop();
		}
	};

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
		, m_SceneHierarchyPanel()
		, m_ContentBrowserPanel(*this)
		, m_Window(Application::Get().GetWindow())
	{
		m_SimulatePanelSettings.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoTabBar;
	}

	void EditorLayer::OnAttach()
	{
		EG_CORE_ASSERT(!s_EditorLayer);
		s_EditorLayer = this;

		EditorResources::Init();

		m_ImGuiLayer = Application::Get().GetImGuiLayer();

		CheckAppAssembly();

		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
		m_GuizmoMode = ImGuizmo::MODE::WORLD;

		m_WindowTitle = "Eagle Editor";
		m_OpenedSceneCallbackID = Scene::AddOnSceneOpenedCallback([this](const Ref<Scene>& scene)
		{
			if (m_EditorState == EditorState::Edit)
			{
				m_EditorScene = scene;
				Ref<Asset> asset;
				if (AssetManager::Get(m_EditorScene->GetGUID(), &asset))
					m_OpenedSceneAsset = Cast<AssetScene>(asset);

				UpdateEditorTitle(m_OpenedSceneAsset);
			}
			else if (m_EditorState == EditorState::Play)
			{
				if (m_SimulationScene)
					m_SimulationScene->OnRuntimeStop();
				m_SimulationScene = scene;
			}
			SetCurrentScene(scene);
			scene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);

			if (m_EditorState == EditorState::Play)
				scene->OnRuntimeStart();

			EG_CORE_INFO("Opened a scene: {}", !m_OpenedSceneAsset ? "<New Scene>" : scene->GetDebugName());
		});

		// If failed to deserialize, create EditorDefault.ini & open a new scene
		const Path editorIni = Project::GetConfigPath() / "EditorDefault.ini";
		if (EditorSerializer::Deserialize(this, editorIni) == false)
		{
			EditorSerializer::Serialize(this, editorIni);
			EditorSerializer::Deserialize(this, editorIni);
		}
	
		SoundSettings soundSettings;
		soundSettings.VolumeMultiplier = 0.25f;
		m_PlaySound = Sound2D::Create(Audio::Create(FileSystem::Read(Application::GetCorePath() / "assets/audio/playsound.wav")), soundSettings);

		m_PlayButtonIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/playbutton.png");
		m_StopButtonIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/stopbutton.png");
	}

	void EditorLayer::OnDetach()
	{
		EditorSerializer::Serialize(this, Project::GetConfigPath() / "EditorDefault.ini");
		Scene::SetCurrentScene(nullptr);
		Scene::RemoveOnSceneOpenedCallback(m_OpenedSceneCallbackID);
		EditorResources::Release();
		EG_CORE_ASSERT(s_EditorLayer);
		s_EditorLayer = nullptr;
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("EditorLayer. OnUpdate");

		m_Ts = ts;
		m_CurrentScene->bDrawMiscellaneous = m_bDrawEditorMisc;
		m_CurrentScene->bDrawNavMesh = bDrawNavMesh;

		{
			std::scoped_lock lock(s_DeferredCallsMutex);
			for (auto& func : m_DeferredCalls)
				func();
			m_DeferredCalls.clear();
		}

		// We shouldn't render if window is unfocused, and the user requested not to do it in that case.
		const bool bShouldRenderBasedOnFocus = !bRenderOnlyWhenFocused || m_WindowFocused;
		ReloadScriptsIfNecessary();
		HandleResize();
		m_CurrentScene->OnUpdate(ts, !m_ViewportHidden && bShouldRenderBasedOnFocus, bUpdateAnimationsInEditor);
	}

	void EditorLayer::OnEvent(Eagle::Event& e)
	{
		if (m_SceneHierarchyPanel.OnEvent(e, IsViewportFocused()))
		{
			if (m_EditorState == EditorState::Edit && m_OpenedSceneAsset)
				m_OpenedSceneAsset->SetDirty(true);
		}

		if (e.Handled)
			return;

		m_ContentBrowserPanel.OnEvent(e);
		if (e.Handled)
			return;

		if (!m_ViewportHidden)
		{
			if (m_EditorState == EditorState::Edit)
				m_EditorScene->OnEventEditor(e);
			else if (m_EditorState == EditorState::Play)
				m_SimulationScene->OnEventRuntime(e);
		}

		if (e.GetEventType() == EventType::WindowFocused)
			m_WindowFocused = ((WindowFocusedEvent&)e).IsFocused();
		else if (e.GetEventType() == EventType::WindowClose)
		{
			e.Handled = true;
			WindowCloseEvent& closeEvent = (WindowCloseEvent&)e;
			if (closeEvent.IsQuitGame() && m_EditorState == EditorState::Play)
			{
				// Quit game was requested from C# scripts.
				// But since we're in the editor, we just need to stop the simulation.
				// But do it when the next frame starts to avoid corrupting the current frame logic
				Submit([this]()
				{
					StopPlayingScene();
				});
			}
			else
			{
				HandleCloseRequest(true);
			}
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(EG_BIND_FN(EditorLayer::HandleEntitySelection));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("EditorLayer. UI");

		const auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		const GBuffer& gBuffers = sceneRenderer->GetGBuffer();
		m_ViewportImage = &(GetRequiredGBufferImage(sceneRenderer, gBuffers));

		BeginDocking();

		if (!m_bFullScreen)
		{
			DrawMenuBar();
			DrawSceneSettings();
			DrawRendererSettings();
			DrawProjectSettings();
			DrawEditorPreferences();
			DrawStats();
		}

		DrawViewport();
		
		if (!m_bFullScreen)
		{
			DrawSimulatePanel();
			if (m_SceneHierarchyPanel.OnImGuiRender(m_EditorState == EditorState::Play))
			{
				if (m_EditorState == EditorState::Edit && m_OpenedSceneAsset)
					m_OpenedSceneAsset->SetDirty(true);
			}
			m_ContentBrowserPanel.OnImGuiRender();
			m_ConsolePanel.OnImGuiRender();
			HandleDirtyAssetsPopup();
		}

		if (m_ShowSaveScenePopupForNewScene)
		{
			UI::ButtonType result = UI::ShowMessage("Eagle Editor", "Do you want to save the current scene?", UI::ButtonType::YesNoCancel);
			if (result == UI::ButtonType::Yes)
			{
				if (SaveScene()) // Open a new scene only if the old scene was successfully saved
					NewScene();
				m_ShowSaveScenePopupForNewScene = false;
			}
			else if (result == UI::ButtonType::No)
			{
				NewScene();
				m_ShowSaveScenePopupForNewScene = false;
			}
			else if (result == UI::ButtonType::Cancel)
				m_ShowSaveScenePopupForNewScene = false;
		}

		EndDocking();

		//ImGui::ShowStyleEditor();
		//ImGui::ShowDemoWindow();
		//ImPlot::ShowDemoWindow();
	}

	const EditorLayer* EditorLayer::Get()
	{
		return s_EditorLayer;
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
			return false;

		//Shortcuts
		if (e.GetRepeatCount() > 0)
			return false;

		const bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		const bool leftShift = Input::IsKeyPressed(Key::LeftShift);
		const bool rightShift = Input::IsKeyPressed(Key::RightShift);
		const bool shift = leftShift || rightShift;
		const bool leftAlt = Input::IsKeyPressed(Key::LeftAlt);

		const Key pressedKey = e.GetKey();
		switch (pressedKey)
		{
			case Key::F5:
				Application::Get().CallNextFrame([]()
				{
					ShaderManager::ReloadAllShaders();
				});
				break;

			case Key::N:
				if (control)
					m_ShowSaveScenePopupForNewScene = true;
				break;

			case Key::S:
				if (control && shift)
					SaveSceneAs();
				else if (control)
					SaveScene();
				break;

			case Key::G:
				if (m_ViewportFocused)
					m_bDrawEditorMisc = !m_bDrawEditorMisc;
				break;

			case Key::P:
				if (leftAlt)
					HandleOnSimulationButton();
				break;

			case Key::F11:
			{
				if (leftShift)
				{
					ToggleWindowFullscreenState();
				}
				else
					m_bFullScreen = !m_bFullScreen;
				break;
			}
		}

		if (pressedKey == m_StopSimulationKey && m_EditorState == EditorState::Play)
			HandleOnSimulationButton();

		//Gizmos
		if (m_ViewportHovered && !ImGuizmo::IsUsing())
		{
			switch (pressedKey)
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

	void EditorLayer::LoadAppAssembly()
	{
		const auto& project = Project::GetProjectInfo();
		if (!ScriptEngine::LoadAppAssembly(Project::GetBinariesPath() / (project.Name + ".dll")))
		{
			const std::string error = std::string("Open VS solution (") +
				(project.BasePath / (project.Name + ".sln")).u8string() + " or \"File > Open VS Solution\") and compile the project.\nIf the solution is not there, try to generate it \"File > Generate VS Solution\"";
			m_ImGuiLayer->AddMessage(error);
			EG_CORE_WARN(error);
		}
	}

	void EditorLayer::CheckAppAssembly()
	{
		if (!ScriptEngine::IsValidAppAssembly())
		{
			const auto& project = Project::GetProjectInfo();
			const std::string error = std::string("Open VS solution (") +
				(project.BasePath / (project.Name + ".sln")).u8string() + " or \"File > Open VS Solution\") and compile the project.\nIf the solution is not there, try to generate it \"File > Generate VS Solution\"";
			m_ImGuiLayer->AddMessage(error);
		}
	}

	void EditorLayer::ReloadScriptsIfNecessary()
	{
		static bool bRequiresScriptsRebuild = false;
		if (bRequiresScriptsRebuild || Utils::WereScriptsRebuild())
		{
			if (m_EditorState == EditorState::Edit)
			{
				bRequiresScriptsRebuild = false;
				LoadAppAssembly();
			}
			else
				bRequiresScriptsRebuild = true; // Set it to true since it might be false and `Utils::WereScriptsRebuild()` is triggered only once
		}
	}

	void EditorLayer::HandleResize()
	{
		if (m_NewViewportSize != m_CurrentViewportSize)
		{
			EG_CORE_TRACE("Viewport was resized: {}x{}", m_NewViewportSize.x, m_NewViewportSize.y);
			m_CurrentViewportSize = m_NewViewportSize;
			m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			if (m_SimulationScene)
				m_SimulationScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		}
	}

	bool EditorLayer::HandleEntitySelection(MouseButtonPressedEvent& e)
	{
		if (m_EditorState == EditorState::Play || e.GetMouseCode() != Mouse::ButtonLeft)
			return false;

		//Entity Selection
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		bool bUsingImGuizmo = selectedEntity && (ImGuizmo::IsUsing() || ImGuizmo::IsOver());
		if (m_ViewportHovered && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
		{
			const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			const glm::ivec2 mouse = GetMousePosWithinViewport();

			if (mouse.x >= 0 && mouse.y >= 0 && mouse.x < (int)viewportSize.x && mouse.y < (int)viewportSize.y)
			{
				Ref<Image>& image = m_CurrentScene->GetSceneRenderer()->GetGBuffer().ObjectIDCopy;
				int data = -1;

				const ImageSubresourceLayout imageLayout = image->GetImageSubresourceLayout();
				uint8_t* mapped = (uint8_t*)image->Map();
				mapped += imageLayout.Offset;
				mapped += imageLayout.RowPitch * mouse.y;
				memcpy(&data, ((uint32_t*)mapped) + mouse.x, sizeof(int));
				image->Unmap();
				m_SceneHierarchyPanel.SetEntitySelected(data == -1 ? Entity::Null : Entity{ (entt::entity)data, m_CurrentScene.get() });
				return true;
			}
		}
		return false;
	}

	void EditorLayer::ToggleWindowFullscreenState()
	{
		Window& window = Application::Get().GetWindow();
		bool bFullscreen = window.IsFullscreen();
		if (!bFullscreen)
		{
			m_WindowPosBeforeFS = window.GetWindowPos();
			m_WindowSizeBeforeFS = window.GetWindowSize();
		}
		window.SetFullscreen(!window.IsFullscreen());
		bFullscreen = window.IsFullscreen();
		if (!bFullscreen)
		{
			window.SetWindowPos(int(m_WindowPosBeforeFS.x), int(m_WindowPosBeforeFS.y));
			window.SetWindowSize(int(m_WindowSizeBeforeFS.x), int(m_WindowSizeBeforeFS.y));
		}
	}

	void EditorLayer::HandleEntityDragDrop()
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GetAssetDragDropCellTag(AssetType::Entity)))
			{
				const wchar_t* payload_n = (const wchar_t*)payload->Data;
				Path filepath(payload_n);

				Ref<Asset> asset;
				if (AssetManager::Get(filepath, &asset))
				{
					Ref<AssetEntity> entityAsset = Cast<AssetEntity>(asset);

					const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
					const glm::ivec2 mouse = GetMousePosWithinViewport();

					if (mouse.x >= 0 && mouse.y >= 0 && mouse.x < (int)viewportSize.x && mouse.y < (int)viewportSize.y)
					{
						RenderManager::Submit([editorLayer = this, mouse, entityAsset](Ref<CommandBuffer>&)
						{
							Ref<Image>& depthBuffer = editorLayer->m_CurrentScene->GetSceneRenderer()->GetGBuffer().Depth;
							float depth = 0.f;
							const ImageLayout depthLayout = depthBuffer->GetLayout();
							depthBuffer->Read(&depth, sizeof(float), glm::ivec3{ mouse.x, mouse.y, 0 }, glm::uvec3{ 1 }, depthLayout, depthLayout);

							const glm::vec2 uv = glm::vec2(mouse.x, mouse.y) / glm::vec2(depthBuffer->GetSize());

							editorLayer->Submit([editorLayer, depth, entityAsset, uv]()
							{
								editorLayer->SpawnEntityAtDepth(entityAsset, uv, depth);
							});
						});
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void EditorLayer::SpawnEntityAtDepth(const Ref<AssetEntity>& entityAsset, glm::vec2 uv, float depth)
	{
		const auto& editorCamera = m_EditorScene->GetEditorCamera();
		glm::vec3 worldPos = Math::WorldPosFromDepth(glm::inverse(editorCamera.GetViewProjection()), uv, depth);
		
		if (depth == 0.f)
		{
			const auto& cameraPos = editorCamera.GetLocation();
			worldPos = cameraPos + glm::normalize(worldPos - cameraPos);
		}

		Entity createdEntity = m_EditorScene->CreateFromEntityAsset(entityAsset);
		createdEntity.SetWorldLocation(worldPos);
		m_SceneHierarchyPanel.SetEntitySelected(createdEntity);
		if (m_OpenedSceneAsset)
			m_OpenedSceneAsset->SetDirty(true);
	}

	glm::ivec2 EditorLayer::GetMousePosWithinViewport() const
	{
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;

		return glm::ivec2(mx, my);
	}

	void EditorLayer::NewScene()
	{
		if (m_EditorState == EditorState::Edit)
		{
			m_OpenedSceneAsset.reset();
			Scene::OpenScene(nullptr);
		}
	}

	void EditorLayer::OpenScene(const Ref<AssetScene>& sceneAsset)
	{
		if (m_EditorState == EditorState::Edit)
			Scene::OpenScene(sceneAsset);
	}

	// TODO: Don't use file dialog because user can save it outside of Content folder
	bool EditorLayer::SaveScene()
	{
		if (m_EditorState != EditorState::Edit)
			return false;

		if (!m_OpenedSceneAsset)
		{
			Path filepath = FileDialog::SaveFile(FileDialog::ASSET_FILTER, Project::GetContentPath());
			if (!filepath.empty())
			{
				const Path currentPath = Project::GetProjectPath();
				filepath = std::filesystem::relative(filepath, currentPath);
				const bool bDir = std::filesystem::is_directory(filepath);
				Path assetPath = AssetImporter::CreateScene(bDir ? filepath : filepath.parent_path(), bDir ? "NewScene" : filepath.stem().u8string());

				Ref<Asset> asset;
				if (AssetManager::Get(assetPath, &asset) == false)
				{
					m_ImGuiLayer->AddMessage("Error opening a scene. It's not a scene asset");
					EG_CORE_ERROR("Error opening a scene. It's not a scene asset {0}", assetPath.u8string());
					return false;
				}

				Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset);
				if (!sceneAsset)
				{
					m_ImGuiLayer->AddMessage("Error opening a scene. It's not a scene asset");
					EG_CORE_ERROR("Error opening a scene. It's not a scene asset {0}", assetPath.u8string());
					return false;
				}
				
				m_EditorScene->SetGUID(sceneAsset->GetGUID());
				SceneSerializer::Serialize(m_EditorScene, assetPath);

				m_OpenedSceneAsset = sceneAsset;
				UpdateEditorTitle(m_OpenedSceneAsset);
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (SceneSerializer::Serialize(m_EditorScene, m_OpenedSceneAsset->GetPath()))
				m_OpenedSceneAsset->SetDirty(false);
		}
		return true;
	}

	bool EditorLayer::SaveSceneAs()
	{
		if (m_EditorState != EditorState::Edit)
			return false;

		Path filepath = FileDialog::SaveFile(FileDialog::ASSET_FILTER, Project::GetContentPath());
		if (!filepath.empty())
		{
			const Path currentPath = Project::GetProjectPath();
			filepath = std::filesystem::relative(filepath, currentPath);
			const bool bDir = std::filesystem::is_directory(filepath);
			Path assetPath = AssetImporter::CreateScene(bDir ? filepath : filepath.parent_path(), bDir ? "NewScene" : filepath.stem().u8string());

			Ref<Asset> asset;
			if (AssetManager::Get(assetPath, &asset) == false)
			{
				m_ImGuiLayer->AddMessage("Error opening a scene. It's not a scene asset");
				EG_CORE_ERROR("Error opening a scene. It's not a scene asset {0}", assetPath.u8string());
				return false;
			}

			Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset);
			if (!sceneAsset)
			{
				m_ImGuiLayer->AddMessage("Error opening a scene. It's not a scene asset");
				EG_CORE_ERROR("Error opening a scene. It's not a scene asset {0}", assetPath.u8string());
				return false;
			}

			m_EditorScene->SetGUID(sceneAsset->GetGUID());
			SceneSerializer::Serialize(m_EditorScene, assetPath);

			m_OpenedSceneAsset = sceneAsset;
			UpdateEditorTitle(m_OpenedSceneAsset);
			return true;
		}

		return false;
	}

	void EditorLayer::UpdateEditorTitle(const Ref<AssetScene>& scene)
	{
		if (!scene)
		{
			m_Window.SetWindowTitle(m_WindowTitle + std::string(" - <New Scene>"));
			return;
		}

		std::string displayName = scene->GetPath().u8string();
		const size_t contentPos = displayName.find("Content");
		if (contentPos != std::string::npos)
			displayName = displayName.substr(contentPos);

		m_Window.SetWindowTitle(m_WindowTitle + std::string(" - ") + displayName);
	}

	void EditorLayer::OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, const SceneRendererSettings& settings, bool bWindowMaximized, bool bVSync,
		bool bRenderOnlyWhenFocused, bool bDrawNavMesh, Key stopSimulationKey, bool bUpdateAnimationsInEditor, int guizmoMode)
	{
		// Scene creation needs to go through this way of setting it up since we need to get Ref<Scene> immediately
		m_EditorScene = MakeRef<Scene>("Editor Scene");
		SetCurrentScene(m_EditorScene);
		if (m_OpenedSceneAsset)
		{
			EG_CORE_TRACE("Loading scene '{0}'", m_OpenedSceneAsset->GetPath().u8string());

			if (SceneSerializer::Deserialize(m_EditorScene, m_OpenedSceneAsset->GetPath()))
				EG_CORE_TRACE("Loaded scene '{0}'", m_OpenedSceneAsset->GetPath().u8string());
			UpdateEditorTitle(m_OpenedSceneAsset);
		}
		else
		{
			m_OpenedSceneAsset.reset();
			UpdateEditorTitle(nullptr);
		}
		m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);

		Window& window = Application::Get().GetWindow();
		window.SetVSync(bVSync);
		ImGuiLayer::SelectStyle(m_EditorStyle);
		this->bRenderOnlyWhenFocused = bRenderOnlyWhenFocused;
		this->bUpdateAnimationsInEditor = bUpdateAnimationsInEditor;
		this->bDrawNavMesh = bDrawNavMesh;
		m_GuizmoMode = guizmoMode;
		m_StopSimulationKey = stopSimulationKey;

		if ((int)windowSize.x > 0 && (int)windowSize.y > 0)
		{
			window.SetWindowSize((int)windowSize[0], (int)windowSize[1]);
		}
		if (windowPos.x >= 0 && windowPos.y >= 0)
		{
			window.SetWindowPos((int)windowPos.x, (int)windowPos.y);
		}
		window.SetWindowMaximized(bWindowMaximized);
		
		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		sceneRenderer->SetOptions(settings);
	}

	void EditorLayer::SetCurrentScene(const Ref<Scene>& scene)
	{
		m_CurrentScene = scene;
		Scene::SetCurrentScene(m_CurrentScene);
		m_SceneHierarchyPanel.SetContext(m_CurrentScene);
	}

	void EditorLayer::UpdateGuizmo()
	{
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		SceneComponent* selectedComponent = m_SceneHierarchyPanel.GetSelectedComponent();
		if (selectedComponent)
		{
			const auto selectedType = m_SceneHierarchyPanel.GetSelectedComponentType();
			if (selectedType == SelectedComponent::Decal)
			{
				const AABB aabb(glm::vec3(-0.5f), glm::vec3(0.5f));
				m_CurrentScene->DrawAABB(aabb, selectedComponent->GetWorldTransform());
			}
		}

		if (selectedEntity && (m_GuizmoType != -1))
		{
			ImGuizmo::SetID(int(uint64_t(m_CurrentScene.get())));
			//ImGuizmo::SetOrthographic(false); //TODO: Set to true when using Orthographic
			ImGuizmo::SetDrawlist();

			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x, m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			//Camera
			const auto& editorCamera = m_EditorScene->GetEditorCamera();
			const auto runtimeCamera = m_CurrentScene->GetRuntimeCamera();
			const bool bEditing = m_EditorState == EditorState::Edit;
			glm::mat4 cameraProjection = bEditing ? editorCamera.GetProjection() : runtimeCamera->Camera.GetProjection();
			const glm::mat4& cameraViewMatrix = bEditing ? editorCamera.GetViewMatrix() : runtimeCamera->GetViewMatrix();
			cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

			Transform transform;
			Transform finalTransform;
			bool bRelative = false; // Only used for rotations
			if (selectedComponent)
			{
				transform = selectedComponent->GetWorldTransform();
				bRelative = m_GuizmoType == ImGuizmo::OPERATION::ROTATE;
			}
			else
			{
				transform = selectedEntity.GetWorldTransform();
				bRelative = selectedEntity.HasParent() && (m_GuizmoType == ImGuizmo::OPERATION::ROTATE);
			}
			finalTransform = transform;

			// If relative, we only get relative rotation, since other params need to be in world coords.
			// Otherwise, for example, guizmo will be renderer in the position if we used relative location
			if (bRelative)
				transform.Rotation = (selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform()).Rotation;

			int snappingIndex = 0;
			if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				snappingIndex = 1;
			else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
				snappingIndex = 2;

			//Snapping
			const float snapValues[3] = { m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex] };
			const bool bSnap = Input::IsKeyPressed(Key::LeftShift);

			glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
			ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
				(ImGuizmo::MODE)m_GuizmoMode, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				glm::quat newRotation;

				glm::decompose(transformMatrix, transform.Scale3D, newRotation, transform.Location, notUsed1, notUsed2);

				if (m_GuizmoType == ImGuizmo::OPERATION::TRANSLATE)
					finalTransform.Location = transform.Location;
				if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				{
					if (bRelative)
						finalTransform = selectedComponent ? selectedComponent->GetRelativeTransform() : selectedEntity.GetRelativeTransform();
					finalTransform.Rotation = newRotation;
				}
				if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
					finalTransform.Scale3D = transform.Scale3D;

				if (m_OpenedSceneAsset)
					m_OpenedSceneAsset->SetDirty(true);
				if (selectedComponent)
					bRelative ? selectedComponent->SetRelativeTransform(finalTransform) : selectedComponent->SetWorldTransform(finalTransform);
				else
					bRelative ? selectedEntity.SetRelativeTransform(finalTransform) : selectedEntity.SetWorldTransform(finalTransform);
			}
		}
	}

	void EditorLayer::DrawMenuBar()
	{
		static bool bShowGPUMemoryUsage = false;
		static bool bShowGPUTimings = false;
		static bool bShowCPUTimings = false;
		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		const GBuffer& gBuffers = sceneRenderer->GetGBuffer();

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open VS Solution"))
				{
					const auto& projectInfo = Project::GetProjectInfo();
					const Path solutionFile = projectInfo.BasePath / (projectInfo.Name + ".sln");
					if (std::filesystem::exists(solutionFile))
						Utils::OpenInExplorer(solutionFile);
				}
				if (ImGui::MenuItem("Generate VS Solution"))
				{
					Project::GenerateSolution(Project::GetProjectInfo());
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Build the game"))
				{
					if (!Project::GetProjectInfo().GameStartupScene)
					{
						m_ImGuiLayer->AddMessage("Please set a game startup scene in the 'Project Settings' tab");
					}
					else
					{
						PrepareDirtyAssets(DirtyAssetsReason::ProjectBuild);
						if (!m_ShowDirtyAssetMessage) // No unsaved assets, build.
						{
							const Path projectFolder = FileDialog::OpenFolder();
							if (!projectFolder.empty())
								Project::Build(projectFolder);
						}
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Close the project"))
				{
					HandleCloseRequest(false);
				}
				ImGui::Separator();
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					m_ShowSaveScenePopupForNewScene = true;
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
					HandleCloseRequest(true);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Windows"))
			{
				bool bConsoleOpened = m_ConsolePanel.IsOpened();
				bool bFullscreen = Application::Get().GetWindow().IsFullscreen();

				if (ImGui::Checkbox("Console", &bConsoleOpened))
					m_ConsolePanel.SetOpened(bConsoleOpened);
				if (ImGui::Checkbox("Fullscreen (Shift + F11)", &bFullscreen))
					ToggleWindowFullscreenState();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Debug"))
			{
				if (ImGui::BeginMenu("GPU Buffers"))
				{
					const bool bMotion = sceneRenderer->GetOptions().InternalState.bMotionBuffer;
					if (!bMotion && m_VisualizingGBufferType == GBufferVisualizingType::Motion)
					{
						m_SelectedBufferIndex = 0;
						SetVisualizingBufferType(GBufferVisualizingType::Final);
					}

					int radioButtonIndex = 0;

					if (ImGui::RadioButton("Final", &m_SelectedBufferIndex, radioButtonIndex++))
						SetVisualizingBufferType(GBufferVisualizingType::Final);

					if (ImGui::RadioButton("Albedo", &m_SelectedBufferIndex, radioButtonIndex++))
						SetVisualizingBufferType(GBufferVisualizingType::Albedo);

					if (ImGui::RadioButton("Emission", &m_SelectedBufferIndex, radioButtonIndex++))
						SetVisualizingBufferType(GBufferVisualizingType::Emissive);

					if (bMotion)
						if (ImGui::RadioButton("Motion", &m_SelectedBufferIndex, radioButtonIndex++))
							SetVisualizingBufferType(GBufferVisualizingType::Motion);

					if (sceneRenderer->GetOptions().AO == AmbientOcclusion::SSAO)
						if (ImGui::RadioButton("SSAO", &m_SelectedBufferIndex, radioButtonIndex++))
								SetVisualizingBufferType(GBufferVisualizingType::SSAO);

					if (sceneRenderer->GetOptions().AO == AmbientOcclusion::GTAO)
						if (ImGui::RadioButton("GTAO", &m_SelectedBufferIndex, radioButtonIndex++))
								SetVisualizingBufferType(GBufferVisualizingType::GTAO);

					ImGui::EndMenu();
				}

#ifdef EG_CPU_TIMINGS
				UI::Property("Show CPU timings", bShowCPUTimings);
#endif
#ifdef EG_GPU_TIMINGS
				UI::Property("Show GPU timings", bShowGPUTimings);
#endif
				UI::Property("Show GPU memory usage", bShowGPUMemoryUsage);

				SceneRendererSettings options = sceneRenderer->GetOptions();
				if (UI::Property("Visualize CSM", options.bVisualizeCascades, "Red, green, blue, purple. Doesn't work if there's no directional light"))
					sceneRenderer->SetOptions(options);

				ImGui::EndMenu();
			}

			static bool bShowHelp = false;
			if (ImGui::MenuItem("Help"))
				bShowHelp = true;

			if (bShowHelp)
				ShowHelpWindow(&bShowHelp);
			ImGui::EndMenuBar();
		}

#ifdef EG_GPU_TIMINGS
		if (bShowGPUTimings)
		{
			static GPUTimingsContainer timings;
			static bool bPaused = false;

			// Reserve enough left-over height for 1 separator + 1 property text
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 3.f;

			if (!bPaused)
				timings = RenderManager::GetTimings();

			ImGui::Begin("GPU Timings", &bShowGPUTimings);

			if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				UI::BeginPropertyGrid("GPUTimings");

				UI::Text("Pass name", "Time (ms)", "Timings will probably display incorrect data if two or more viewports are rendered (e.g., asset visualization)");
				ImGui::Separator();

				for (auto& data : timings)
					DisplayTiming(data);

				UI::EndPropertyGrid();
			}
			ImGui::EndChild();
			ImGui::Separator();

			UI::BeginPropertyGrid("GPUTimings");
			UI::Property("Pause", bPaused);
			UI::EndPropertyGrid();

			ImGui::End();
		}
#endif

#ifdef EG_CPU_TIMINGS
		if (bShowCPUTimings)
		{
			constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

			static CPUTimingsContainer timingsPerThread;
			static bool bPaused = false;

			if (!bPaused)
				timingsPerThread = Application::Get().GetCPUTimings();

			ImGui::Begin("CPU Timings", &bShowCPUTimings);

			// Reserve enough left-over height for 1 separator + 1 property text
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 3.f;
			if (ImGui::BeginChild("CPUTimingsPerThread", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				for (auto& [threadID, timings] : timingsPerThread)
				{
					const std::string_view threadName = Application::Get().GetThreadName(threadID);
					ImGui::PushID(threadName.data());
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
					ImGui::Separator();
					bool treeOpened = ImGui::TreeNodeEx("CPU Timings", flags, threadName.data());
					ImGui::PopStyleVar();
					if (treeOpened)
					{
						UI::BeginPropertyGrid("CPUTimings");

						UI::Text("Name", "Time (ms)", "Timings will probably display incorrect data if two or more viewports are rendered (e.g., asset visualization)");
						ImGui::Separator();

						for (auto& timing : timings)
							DisplayTiming(timing);

						UI::EndPropertyGrid();

						ImGui::TreePop();
					}
					ImGui::PopID();
				}
			}
			ImGui::EndChild();

			ImGui::Separator();

			UI::BeginPropertyGrid("CPUTimings");
			UI::Property("Pause", bPaused);
			UI::EndPropertyGrid();

			ImGui::End();
		}
#endif

		if (bShowGPUMemoryUsage)
		{
			constexpr float toMBs = 1.f / (1024 * 1024);
			auto stats = Application::Get().GetRenderContext()->GetMemoryStats();

			ImGui::Begin("GPU Memory usage", &bShowGPUMemoryUsage);
			UI::BeginPropertyGrid("GPUMemUsage");

			UI::Text("Resource name", "Size (MBs)");
			ImGui::Separator();

			UI::Text("Total usage: ", std::to_string(uint64_t(stats.Used * toMBs)));
			UI::Text("Free: ", std::to_string(uint64_t(stats.Free * toMBs)));

			ImGui::Separator();

			std::sort(stats.Resources.begin(), stats.Resources.end(), [](const auto& a, const auto& b)
			{
				return a.Size > b.Size;
			});

			for (auto& resource : stats.Resources)
				if (!resource.Name.empty())
					UI::Text(resource.Name, std::to_string(resource.Size * toMBs));

			UI::EndPropertyGrid();
			ImGui::End();
		}
	}

	void EditorLayer::DrawSceneSettings()
	{
		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		bool bChanged = false;

		ImGui::PushID("SceneSettings");
		ImGui::Begin("Scene Settings");

		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Physics Settings", flags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("PhysicsSceneSettings");

				const bool bSimulating = m_EditorState != EditorState::Edit;
				glm::vec3 gravity = m_CurrentScene->GetGravity();
				uint32_t updateRate = m_CurrentScene->GetPhysicsUpdateRate();
				bool bDebugOnPlay = m_CurrentScene->IsPhysicsDebugOnPlayEnabled();
				DebugType debugType = m_CurrentScene->GetPhysicsDebugType();

				if (UI::PropertyDrag("Gravity", gravity, 0.1f))
				{
					m_CurrentScene->SetGravity(gravity);
					bChanged = true;
				}
				if (UI::PropertyDrag("Update Rate", updateRate, 15, 30, 360, "Defines physics update rate (fps)"))
				{
					m_CurrentScene->SetPhysicsUpdateRate(updateRate);
					bChanged = true;
				}

				if (bSimulating)
					UI::PushItemDisabled();

				if (UI::Property("Debug on Play", bDebugOnPlay, "If enabled, debugging session will start when game simulation starts. You need to use PhysX Visual Debugger"))
				{
					m_CurrentScene->SetPhysicsDebugOnPlay(bDebugOnPlay);
					bChanged = true;
				}
				if (UI::ComboEnum("Debug Type", debugType, s_PhysicsDebugTypeHelpMsg))
				{
					m_CurrentScene->SetPhysicsDebugType(debugType);
					bChanged = true;
				}

				if (bSimulating)
					UI::PopItemDisabled();

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Skybox Settings", flags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("IBLSceneSettings");

				auto cubemap = m_CurrentScene->GetSkybox();
				if (EditorResources::DrawAssetSelection("IBL", cubemap))
				{
					m_CurrentScene->SetSkybox(cubemap);
					bChanged = true;
				}

				float iblIntensity = m_CurrentScene->GetSkyboxIntensity();
				if (UI::PropertyDrag("IBL Lighting Intensity", iblIntensity, 0.1f))
				{
					m_CurrentScene->SetSkyboxIntensity(iblIntensity);
					bChanged = true;
				}

				ImGui::Separator();

				auto skySettings = m_CurrentScene->GetSkySettings();
				bool bChangedSky = false;
				int cumulusLayers = skySettings.CumulusLayers;

				bChangedSky |= UI::PropertyDrag("Sky Sun Position", skySettings.SunPos, 0.01f);
				bChangedSky |= UI::PropertyDrag("Sky Intensity", skySettings.SkyIntensity, 0.1f);
				bChangedSky |= UI::PropertyDrag("Sky Scattering", skySettings.Scattering, 0.01f, 0.001f, 0.999f);

				bChangedSky |= UI::Property("Cirrus Clouds", skySettings.bEnableCirrusClouds);
				bChangedSky |= UI::Property("Cumulus Clouds", skySettings.bEnableCumulusClouds);

				bChangedSky |= UI::PropertyColor("Clouds Color", skySettings.CloudsColor);
				bChangedSky |= UI::PropertyDrag("Clouds Intensity", skySettings.CloudsIntensity, 0.1f);

				bChangedSky |= UI::PropertyDrag("Cirrus Clouds Amount", skySettings.Cirrus, 0.01f);
				bChangedSky |= UI::PropertyDrag("Cumulus Clouds Amount", skySettings.Cumulus, 0.01f);
				if (UI::PropertyDrag("Cumulus Clouds Layers", cumulusLayers, 1.f, 1, INT_MAX))
				{
					skySettings.CumulusLayers = uint32_t(cumulusLayers);
					bChangedSky = true;
				}

				if (bChangedSky)
				{
					m_CurrentScene->SetSkybox(skySettings);
					bChanged = true;
				}

				ImGui::Separator();
				bool bUseSkyAsBackground = m_CurrentScene->GetUseSkyAsBackground();
				if (UI::Property("Sky as background", bUseSkyAsBackground, s_SkyHelpMsg))
				{
					m_CurrentScene->SetUseSkyAsBackground(bUseSkyAsBackground);
					bChanged = true;
				}

				bool bRenderSkybox = m_CurrentScene->IsRenderSkyboxEnabled();
				if (UI::Property("Render Skybox", bRenderSkybox, "If disabled, IBL will still light the scene"))
				{
					m_CurrentScene->SetRenderSkybox(bRenderSkybox);
					bChanged = true;
				}

				bool bEnableSkybox = m_CurrentScene->IsSkyboxEnabled();
				if (UI::Property("Enable Skybox", bEnableSkybox, s_SkyboxEnableHelpMsg))
				{
					m_CurrentScene->SetSkyboxEnabled(bEnableSkybox);
					bChanged = true;
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		ImGui::End();
		ImGui::PopID();

		if (bChanged && m_OpenedSceneAsset)
			m_OpenedSceneAsset->SetDirty(true);
	}

	void EditorLayer::DrawRendererSettings()
	{
		ImGui::Begin("Renderer Settings");
		UI::BeginPropertyGrid("RendererSettingsPanel");

		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		SceneRendererSettings options = sceneRenderer->GetOptions();
		bool bSettingsChanged = false;

		bool bVSync = Application::Get().GetWindow().IsVSync();
		if (UI::Property("VSync", bVSync))
		{
			EG_CORE_TRACE("Changed VSync to: {}", bVSync);
			Application::Get().GetWindow().SetVSync(bVSync);
		}

		bSettingsChanged |= UI::PropertyDrag("Gamma", options.Gamma, 0.1f, 0.0f, 10.f);

		if (options.AutoExposure.bEnable)
			UI::PushItemDisabled();
		bSettingsChanged |= UI::PropertyDrag("Exposure", options.Exposure, 0.1f, 0.0f, 100.f, "Disabled, if Auto Exposure is used");
		if (options.AutoExposure.bEnable)
			UI::PopItemDisabled();

		bSettingsChanged |= UI::ComboEnum<TonemappingMethod>("Tonemapping", options.Tonemapping);

		if (UI::Property("Stutterless", options.bStutterlessShaders, s_StutterlessHelpMsg))
		{
			EG_CORE_TRACE("Changed Stutterless to: {}", options.bStutterlessShaders);
			bSettingsChanged = true;
		}

		if (UI::Property("In-game object picking", options.bEnableObjectPicking, "You can disable it through C# when it's not needed to improve performance and reduce memory usage"))
		{
			EG_CORE_TRACE("Changed Object Picking to: {}", options.bEnableObjectPicking);
			bSettingsChanged = true;
		}

		if (UI::Property("In-game 2D object picking", options.bEnable2DObjectPicking, "If set to false, 2D objects will be ignored (for example, Text2D and Image2D components)"))
		{
			EG_CORE_TRACE("Changed 2D Object Picking to: {}", options.bEnable2DObjectPicking);
			bSettingsChanged = true;
		}

		if (UI::Property("Sort opaque particles", options.bSortOpaqueParticles))
		{
			EG_CORE_TRACE("Changed `Sort opaque particles` to: {}", options.bSortOpaqueParticles);
			bSettingsChanged = true;
		}

		if (UI::PropertyDrag("Line width", options.LineWidth, 0.1f))
		{
			options.LineWidth = glm::max(options.LineWidth, 0.f);
			bSettingsChanged = true;
			EG_CORE_TRACE("Changed Line Width to: {}", options.LineWidth);
		}

		if (UI::PropertyDrag("Grad Scale", options.GridScale, 0.1f))
		{
			options.GridScale = glm::max(options.GridScale, 0.f);
			bSettingsChanged = true;
			EG_CORE_TRACE("Changed Grad Scale to: {}", options.GridScale);
		}

		int transparencyLayers = options.TransparencyLayers;
		if (UI::PropertyDrag("Transparency Layers", transparencyLayers, 1.f, 1, 16, s_TransparencyLayersHelpMsg))
		{
			options.TransparencyLayers = uint32_t(transparencyLayers);
			bSettingsChanged = true;
			EG_CORE_TRACE("Changed Transparency Layers to: {}", options.TransparencyLayers);
		}

		// Ambient Occlusion method
		{
			const AmbientOcclusion oldAO = options.AO;
			if (UI::ComboEnum<AmbientOcclusion>("Ambient Occlusion", options.AO))
			{
				bSettingsChanged = true;
				EG_CORE_TRACE("Changed AO to: {}", magic_enum::enum_name(options.AO));

				if (options.AO != AmbientOcclusion::SSAO && oldAO == AmbientOcclusion::SSAO)
				{
					if (m_VisualizingGBufferType == GBufferVisualizingType::SSAO)
					{
						SetVisualizingBufferType(GBufferVisualizingType::Final);
						m_SelectedBufferIndex = 0;
						m_ViewportImage = &sceneRenderer->GetOutput();
					}
				}
				else if (options.AO != AmbientOcclusion::GTAO && oldAO == AmbientOcclusion::GTAO)
				{
					if (m_VisualizingGBufferType == GBufferVisualizingType::GTAO)
					{
						SetVisualizingBufferType(GBufferVisualizingType::Final);
						m_SelectedBufferIndex = 0;
						m_ViewportImage = &sceneRenderer->GetOutput();
					}
				}
			}
		}

		if (UI::ComboEnum<AAMethod>("Anti-aliasing", options.AA))
		{
			bSettingsChanged = true;
			EG_CORE_TRACE("Changed AA to: {}", magic_enum::enum_name(options.AA));
		}

		UI::EndPropertyGrid();

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		// Auto Exposure settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Auto Exposure", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Auto Exposure Settings");

				auto& settings = options.AutoExposure;

				bSettingsChanged |= UI::Property("Enable", settings.bEnable);
				bSettingsChanged |= UI::Property("Half Resolution", settings.bHalfResolution, "If set to true, histogram calculation are performed in half-res, hence improving performance by the cost of the quality.");
				bSettingsChanged |= UI::PropertyDrag("Min Luminance (log)", settings.MinLogLum, 0.1f, 0.f, 0.f, "Logarithmic value");
				bSettingsChanged |= UI::PropertyDrag("Max Luminance (log)", settings.MaxLogLum, 0.1f, 0.f, 0.f, "Logarithmic value");
				if (UI::PropertyDrag("Adaptation Speed", settings.AdaptationSpeed, 0.05f, 0.f, 0.f, "Controls how fast Auto Exposure reacts to changes"))
				{
					settings.AdaptationSpeed = glm::max(settings.AdaptationSpeed, 0.f);
					bSettingsChanged = true;
				}
				if (UI::PropertyDrag("Adaptation Key", settings.AdaptationKey, 0.01f, 0.f, 0.f, "Controls the final Exposure"))
				{
					settings.AdaptationKey = glm::max(settings.AdaptationKey, 0.f);
					bSettingsChanged = true;
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Shadow Resolutions settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Shadows", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Shadow Settings");

				bSettingsChanged |= UI::Property("Enable Translucent Shadows", options.bTranslucentShadows);
				bSettingsChanged |= UI::Property("Enable Soft Shadows", options.bEnableSoftShadows, "Hard shadows are still filtered using 3x3 PCF filter");
				bSettingsChanged |= UI::Property("Enable Shadows smooth transition", options.bEnableCSMSmoothTransition, "Enable smooth transition of cascaded shadows (affects shadows that are casted by directional light)");

				auto& editorCamera = m_CurrentScene->GetEditorCamera();
				float maxShadowDist = editorCamera.GetShadowFarClip();
				if (UI::PropertyDrag("Max Shadow distance", maxShadowDist, 1.f, 0.f, 0.f, s_MaxShadowDistHelpMsg))
					editorCamera.SetShadowFarClip(maxShadowDist);

				float cascadesSplitAlpha = editorCamera.GetCascadesSplitAlpha();
				if (UI::PropertySlider("Cascades Split Alpha", cascadesSplitAlpha, 0.f, 1.f, s_CascadesSplitAlphaHelpMsg))
					editorCamera.SetCascadesSplitAlpha(cascadesSplitAlpha);

				float csmTransitionAlpha = editorCamera.GetCascadesSmoothTransitionAlpha();
				if (UI::PropertySlider("Cascades Smooth Transition Alpha", csmTransitionAlpha, 0.f, 1.f, s_CascadesSmoothTransitionAlphaHelpMsg))
					editorCamera.SetCascadesSmoothTransitionAlpha(csmTransitionAlpha);

				ImGui::Separator();

				ShadowMapsSettings& settings = options.ShadowsSettings;
				if (UI::PropertyDrag("Point Light ShadowMap Size", settings.PointLightShadowMapSize, 32.f, 0, 16384))
				{
					settings.PointLightShadowMapSize = glm::max(settings.PointLightShadowMapSize, ShadowMapsSettings::MinPointLightShadowMapSize);
					EG_CORE_TRACE("Point Light ShadowMap Size changed to: {}", settings.PointLightShadowMapSize);
					bSettingsChanged = true;
				}

				if (UI::PropertyDrag("Spot Light ShadowMap Size", settings.SpotLightShadowMapSize, 32.f, 0, 16384))
				{
					settings.SpotLightShadowMapSize = glm::max(settings.SpotLightShadowMapSize, ShadowMapsSettings::MinSpotLightShadowMapSize);
					EG_CORE_TRACE("Spot Light ShadowMap Size changed to: {}", settings.SpotLightShadowMapSize);
					bSettingsChanged = true;
				}

				ImGui::Separator();

				for (uint32_t i = 0; i < RendererConfig::CascadesCount; ++i)
				{
					const std::string name = std::string("Dir Light ShadowMap Size #") + std::to_string(i + 1);
					if (UI::PropertyDrag(name, settings.DirLightShadowMapSizes[i], 32.f, 0, 16384))
					{
						settings.DirLightShadowMapSizes[i] = glm::max(settings.DirLightShadowMapSizes[i], ShadowMapsSettings::MinDirLightShadowMapSize);
						EG_CORE_TRACE("{} changed to: {}", name, settings.DirLightShadowMapSizes[i]);
						bSettingsChanged = true;
					}
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Screen Space Reflections settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Screen-space Reflections", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Screen-space Reflections Settings");

				auto& settings = options.ScreenSpaceReflections;

				bSettingsChanged |= UI::Property("Enable", settings.bEnable);
				bSettingsChanged |= UI::PropertySlider("Roughness Threshold", settings.RoughnessThreshold, 0.f, 1.f);
				if (UI::PropertySlider("Samples Per Quad", settings.SamplesPerQuad, 1, 4, "1, 2, or 4"))
				{
					if (settings.SamplesPerQuad == 3)
						settings.SamplesPerQuad = 4;
					bSettingsChanged = true;
				}
				bSettingsChanged |= UI::PropertyDrag("Max Traversal Iterations", settings.MaxTraversalIterations, 1, 1, 512);

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Bloom settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Bloom", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Bloom Settings");

				BloomSettings& settings = options.BloomSettings;
				if (UI::Property("Enable Bloom", settings.bEnable))
				{
					EG_CORE_TRACE("Enabled Bloom: {}", settings.bEnable);
					bSettingsChanged = true;
				}

				if (UI::PropertyDrag("Threshold", settings.Threshold, 0.05f))
				{
					settings.Threshold = std::max(0.f, settings.Threshold);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Bloom Threshold to: {}", settings.Threshold);
				}
				if (UI::PropertyDrag("Intensity", settings.Intensity, 0.05f))
				{
					settings.Intensity = std::max(0.f, settings.Intensity);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Bloom Intensity to: {}", settings.Intensity);
				}
				if (UI::PropertyDrag("Dirt Intensity", settings.DirtIntensity, 0.05f))
				{
					settings.DirtIntensity = std::max(0.f, settings.DirtIntensity);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Bloom Dirt Intensity to: {}", settings.DirtIntensity);
				}
				if (UI::PropertyDrag("Knee", settings.Knee, 0.01f))
				{
					settings.Knee = std::max(0.f, settings.Knee);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Bloom Knee to: {}", settings.Knee);
				}
				if (EditorResources::DrawAssetSelection("Dirt", settings.Dirt))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Bloom Dirt Texture to: {}", settings.Dirt ? settings.Dirt->GetPath().u8string() : "None");
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}
		
		// SSAO settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("SSAO", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("SSAO Settings");

				SSAOSettings& settings = options.SSAOSettings;
				int samples = (int)settings.GetNumberOfSamples();
				float radius = settings.GetRadius();
				float bias = settings.GetBias();

				if (UI::PropertyDrag("Samples", samples, 2, 2, INT_MAX))
				{
					settings.SetNumberOfSamples(uint32_t(samples));
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed SSAO Samples Number to: {}", settings.GetNumberOfSamples());
				}
				if (UI::PropertyDrag("Radius", radius, 0.01f))
				{
					settings.SetRadius(radius);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed SSAO Radius to: {}", settings.GetRadius());
				}
				if (UI::PropertyDrag("Bias", bias, 0.01f))
				{
					settings.SetBias(bias);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed SSAO Bias to: {}", settings.GetBias());
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// GTAO settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("GTAO", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("GTAO Settings");

				GTAOSettings& settings = options.GTAOSettings;
				int samples = (int)settings.GetNumberOfSamples();
				float radius = settings.GetRadius();

				if (UI::PropertyDrag("Samples", samples, 1, 1, INT_MAX))
				{
					settings.SetNumberOfSamples(uint32_t(samples));
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed GTAO Samples Number to: {}", settings.GetNumberOfSamples());
				}
				if (UI::PropertyDrag("Radius", radius, 0.01f))
				{
					settings.SetRadius(radius);
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed GTAO Radius to: {}", settings.GetRadius());
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Volumetric settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Volumetric Lights", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Volumetric Lights Settings");

				VolumetricLightsSettings& settings = options.VolumetricSettings;
				if (UI::Property("Enable Volumetric Lights", settings.bEnable, s_EnableVolumetricLightsHelpMsg))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Enabled Volumetric Lights: {}", settings.bEnable);
				}

				if (UI::Property("Enable Volumetric Fog", settings.bFogEnable))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Enabled Volumetric Fog: {}", settings.bFogEnable);
				}

				if (UI::PropertyDrag("Samples", settings.Samples, 1.f, 1, 0, "Use with caution! Making it to high might kill the performance. Especially if the light casts shadows"))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Volumetric Samples to: {}", settings.Samples);
				}
				if (UI::PropertyDrag("Max Scattering Distance", settings.MaxScatteringDistance, 1.f, 0.f, FLT_MAX))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Volumetric Max Scattering Distance to: {}", settings.MaxScatteringDistance);
				}
				if (UI::PropertyDrag("Fog Speed", settings.FogSpeed, 0.01f))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Volumetric Fog Speed to: {}", settings.FogSpeed);
				}
				if (UI::PropertyColor("Albedo", settings.Albedo))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Volumetric Albedo to: {}", settings.Albedo);
				}
				if (UI::PropertyDrag("Anisotropy", settings.Anisotropy, 0.05f, -1.f, 1.f))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Changed Volumetric Anisotropy to: {}", settings.Anisotropy);
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Fog settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Fog", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Fog Settings");

				FogSettings& settings = options.FogSettings;
				if (UI::Property("Enable Fog", settings.bEnable))
				{
					bSettingsChanged = true;
					EG_CORE_TRACE("Enabled Fog: {}", settings.bEnable);
				}

				bSettingsChanged |= UI::ComboEnum<FogEquation>("Equation", settings.Equation);
				bSettingsChanged |= UI::PropertyColor("Color", settings.Color);
				bSettingsChanged |= UI::PropertyDrag("Min Distance", settings.MinDistance, 0.5f, 0.f, 0.f, "Everything closer won't be affected by the fog. Used by Linear equation");
				bSettingsChanged |= UI::PropertyDrag("Max Distance", settings.MaxDistance, 0.5f, 0.f, 0.f, "Everything after this distance is fog. Used by Linear equation");
				bSettingsChanged |= UI::PropertyDrag("Density", settings.Density, 0.001f, 0.f, 0.f, "Used by Exponential equations");

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// DOF settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Depth of Field", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("DOF Settings");

				auto& settings = options.DOFSettings;

				bSettingsChanged |= UI::PropertyDrag("Aperture Shape X", settings.ApertureShape.x, 0.1f, 0.f, 2.f);
				bSettingsChanged |= UI::PropertyDrag("Aperture Shape Y", settings.ApertureShape.y, 0.1f, 0.f, 2.f);
				bSettingsChanged |= UI::PropertyDrag("Aperture Size", settings.ApertureSize, 0.01f, 0.f, FLT_MAX);
				bSettingsChanged |= UI::PropertyDrag("Focal Length", settings.FocalLength, 0.01f);
				bSettingsChanged |= UI::PropertyDrag("COC Scale", settings.COCScale, 0.1f, 0.f, FLT_MAX, "Circle of Confusion scale");
				bSettingsChanged |= UI::PropertyDrag("Max COC", settings.MaxCOC, 0.1f, 0.f, FLT_MAX, "Max Circle of Confusion");
				bSettingsChanged |= UI::Property("Debug Output", settings.bDebugOutput);

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Motion Blur settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Motion Blur", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Motion Blur Settings");

				auto& settings = options.MotionBlur;

				bSettingsChanged |= UI::Property("Enable", settings.bEnable);
				bSettingsChanged |= UI::PropertySlider("Num Samples", settings.NumSamples, 1u, 64u);
				bSettingsChanged |= UI::PropertySlider("Strength", settings.Strength, 0.f, 1.f);
				bSettingsChanged |= UI::Property("Debug Output", settings.bDebugOutput);

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Photo Linear Tonemapping Settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Photo Linear tonemapping", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("PhotoSettings");

				auto params = options.PhotoLinearTonemappingParams;
				bool bChanged = false;
				if (UI::PropertyDrag("Sensitivity", params.Sensitivity, 0.01f))
				{
					EG_CORE_TRACE("Changed Photo Linear sensitivity to: {}", params.Sensitivity);
					bChanged = true;
				}
				if (UI::PropertyDrag("Exposure time (sec)", params.ExposureTime, 0.01f))
				{
					EG_CORE_TRACE("Changed Photo Linear Exposure time to: {}", params.ExposureTime);
					bChanged = true;
				}
				if (UI::PropertyDrag("F-Stop", params.FStop, 0.01f))
				{
					EG_CORE_TRACE("Changed Photo Linear F-Stop to: {}", params.FStop);
					bChanged = true;
				}

				if (bChanged)
					options.PhotoLinearTonemappingParams = params;

				bSettingsChanged |= bChanged;

				ImGui::TreePop();
				UI::EndPropertyGrid();
			}
		}

		// Filmic Tonemapping settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Filmic tonemapping", treeFlags);
			ImGui::PopStyleVar();

			if (treeOpened)
			{
				UI::BeginPropertyGrid("FilmicSettings");

				if (UI::PropertyDrag("White Point", options.FilmicTonemappingParams.WhitePoint, 0.05f))
				{
					EG_CORE_TRACE("Changed Filmic White Point to: {}", options.FilmicTonemappingParams.WhitePoint);
					bSettingsChanged = true;
				}
				ImGui::TreePop();

				UI::EndPropertyGrid();
			}
		}

		if (bSettingsChanged)
			sceneRenderer->SetOptions(options);

		ImGui::End(); //Settings
	}

	void EditorLayer::DrawProjectSettings()
	{
		const auto& projectInfo = Project::GetProjectInfo();
		Ref<AssetScene> startupScene = projectInfo.GameStartupScene;
		glm::uvec3 version = projectInfo.Version;
		const bool bRuntime = m_EditorState != EditorState::Edit;

		if (bRuntime)
			UI::PushItemDisabled();

		ImGui::Begin("Project Settings");
		UI::BeginPropertyGrid("ProjectSettingsPanel");

		bool bChanged = false;
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

		UI::EndPropertyGrid();

		// Physics Collision groups
		{
			constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			const bool treeOpened = ImGui::TreeNodeEx("Collision Groups", treeFlags);
			ImGui::PopStyleVar();

			if (treeOpened)
			{
				UI::BeginPropertyGrid("ProjectSettingsPanel");

				auto userGroups = Project::GetUserCollisionGroups();

				for (auto it = userGroups.begin(); it != userGroups.end(); ++it)
				{
					auto& name = it->first;
					auto& mask = it->second;

					ImGui::PushID(mask);
					if (ImGui::Button("Delete"))
					{
						Project::RemoveUserCollisionGroup(mask);
						m_CurrentScene->InvalidateCollisionGroups(Project::GetValidCollisionGroupsMask());
						m_OpenedSceneAsset->SetDirty(true);
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
					m_CurrentScene->InvalidateCollisionGroups(Project::GetValidCollisionGroupsMask());
					m_OpenedSceneAsset->SetDirty(true);
					bChanged = true;
				}
				if (!bCanAdd)
					UI::PopItemDisabled();

				ImGui::TreePop();
			}
		}

		if (bRuntime)
			UI::PopItemDisabled();

		if (bChanged)
			Project::Save();

		ImGui::End();
	}
	
	void EditorLayer::DrawEditorPreferences()
	{
		constexpr uint64_t treeID = 95242191ull;
		glm::vec3 tempSnappingValues = m_SnappingValues;
		ImGuizmo::MODE guizmoMode = (ImGuizmo::MODE)m_GuizmoMode;
		ImGui::Begin("Editor Preferences");

		constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		bool treeOpened = ImGui::TreeNodeEx((void*)treeID, flags, "Snapping");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("EditorPreferences_Snapping");
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

		{
			UI::BeginPropertyGrid("EditorPreferences");

			if (UI::ComboEnum("Guizmo Mode", guizmoMode))
			{
				m_GuizmoMode = guizmoMode;
			}
			UI::Property("Eco Rendering", bRenderOnlyWhenFocused, "If checked, the scene won't be rendered if the window is not in focus");
			UI::Property("Update Animations", bUpdateAnimationsInEditor, "If checked, animations will be updated in the editor mode");
			UI::Property("Draw Editor Miscellaneous", m_bDrawEditorMisc);
			UI::Property("Draw Nav Mesh", bDrawNavMesh);
			UI::ComboEnum<Eagle::Key>("Stop simulation key", m_StopSimulationKey, "The editor will stop the game-simulation when this key is pressed. Set it to 'None' to disable");
			ImGuiLayer::ShowStyleSelector("Style", m_EditorStyle);

			UI::EndPropertyGrid();
		}

		ImGui::End(); //Editor Preferences
	}
	
	void EditorLayer::DrawStats()
	{
		if (ImGui::Begin("Stats"))
		{
			ImGui::PushID("RendererStats");
			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

			bool renderer3DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer3D", flags, "Renderer3D Stats");
			if (renderer3DTreeOpened)
			{
				const auto& stats = m_CurrentScene->GetSceneRenderer()->GetStats();

				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Vertices: %d", stats.Vertices);
				ImGui::Text("Indices: %d", stats.Indeces);

				ImGui::TreePop();
			}

			bool renderer2DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer2D", flags, "Renderer2D Stats");
			if (renderer2DTreeOpened)
			{
				const auto& stats = m_CurrentScene->GetSceneRenderer()->GetStats2D();

				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Quads: %d", stats.QuadCount);
				ImGui::Text("Vertices: %d", stats.GetVertexCount());
				ImGui::Text("Indices: %d", stats.GetIndexCount());

				ImGui::TreePop();
			}

			ImGui::Text("Frame Time: %.6fms", m_Ts * 1000.f);
			ImGui::Text("FPS: %d", int(1.f / m_Ts));
			ImGui::PopID();
		}
		ImGui::End(); //Stats
	}
	
	void EditorLayer::DrawViewport()
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
			m_CurrentScene->ViewportBounds[0] = m_ViewportBounds[0]; // TODO: Fix for game builds
			m_CurrentScene->ViewportBounds[1] = m_ViewportBounds[1];

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail(); // Getting viewport size
			m_NewViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y); //Converting it to glm::vec2

			UI::Image(*m_ViewportImage, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y });

			// Drop event
			if (m_EditorState == EditorState::Edit)
				HandleEntityDragDrop();

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				ImGui::SetWindowFocus();

			m_ViewportHovered = ImGui::IsWindowHovered();
			m_ViewportFocused = ImGui::IsWindowFocused();

			if (m_EditorState == EditorState::Edit)
			{
				if (ImGui::IsMouseReleased(1) || !m_ViewportFocused)
					m_EditorScene->bCanUpdateEditorCamera = false;
				else if (m_EditorScene->bCanUpdateEditorCamera || (m_ViewportHovered && ImGui::IsMouseClicked(1, true)))
					m_EditorScene->bCanUpdateEditorCamera = true;
			}
		}
		else
			m_EditorScene->bCanUpdateEditorCamera = false;

		if (m_EditorState == EditorState::Edit)
			UpdateGuizmo();

		ImGui::End(); //Viewport
		ImGui::PopStyleVar();
	}
	
	void EditorLayer::DrawSimulatePanel()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.305f, 0.31f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.1505f, 0.151f, 0.5f));
		const Ref<Texture2D>& btnTexture = m_EditorState == EditorState::Edit ? m_PlayButtonIcon : m_StopButtonIcon;

		ImGui::SetNextWindowClass(&m_SimulatePanelSettings);
		ImGui::Begin("##tool_bar", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
		const float size = ImGui::GetWindowHeight() - 10.0f;
		ImGui::SameLine((ImGui::GetWindowContentRegionMax().x / 2.0f) - (1.5f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)) - (size / 2.0f));
		if (UI::ImageButton(btnTexture, { size, size }))
			HandleOnSimulationButton();

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	UI::ButtonType EditorLayer::DrawDirtyAssetsPopup(std::string_view yesButtonText, std::string_view noButtonText)
	{
		UI::ButtonType result = UI::ButtonType::None;

		if (m_ShowDirtyAssetMessage)
		{
			ImGui::OpenPopup("Unsaved assets");

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImVec2 size = ImVec2(720.f, 560.f);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		}

		if (ImGui::BeginPopupModal("Unsaved assets", &m_ShowDirtyAssetMessage))
		{
			ImGui::Text("You have unsaved assets. Select assets to save:");

			ImGui::Separator();

			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 3.f;
			if (ImGui::BeginChild("DirtyAssetsScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				const size_t size = m_DirtyAssets.size();
				for (size_t i = 0; i < size; ++i)
				{
					const auto& asset = m_DirtyAssets[i];
					bool bChecked = m_DirtyAssetsChecked[i];
					if (ImGui::Checkbox(asset->GetPath().u8string().c_str(), &bChecked))
					{
						m_DirtyAssetsChecked[i] = bChecked;
					}
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			if (ImGui::Button(yesButtonText.data()))
			{
				result = UI::ButtonType::Yes;
			}

			ImGui::SameLine();

			if (ImGui::Button(noButtonText.data()))
			{
				result = UI::ButtonType::No;
			}

			ImGui::EndPopup();
		}

		return result;
	}

	void EditorLayer::HandleDirtyAssetsPopup()
	{
		std::string_view yesBtnText;
		std::string_view noBtnText;

		if (m_DirtyAssetsReason == DirtyAssetsReason::ProjectClose)
		{
			yesBtnText = "Save and exit";
			noBtnText = "Ignore and exit";
		}
		else if (m_DirtyAssetsReason == DirtyAssetsReason::ProjectBuild)
		{
			yesBtnText = "Save and build";
			noBtnText = "Ignore and build";
		}

		UI::ButtonType button = DrawDirtyAssetsPopup(yesBtnText, noBtnText);
		if (button == UI::ButtonType::None)
			return;

		m_ShowDirtyAssetMessage = false;

		if (button == UI::ButtonType::Yes)
		{
			SaveDirtyAssets();
		}

		if (m_DirtyAssetsReason == DirtyAssetsReason::ProjectClose)
		{
			if (m_CloseEngineRequested)
				Application::Get().SetShouldClose(true);
			else
				OpenProjectSelector();

			m_CloseEngineRequested = false;
		}

		if (m_DirtyAssetsReason == DirtyAssetsReason::ProjectBuild)
		{
			const Path projectFolder = FileDialog::OpenFolder();
			if (!projectFolder.empty())
				Project::Build(projectFolder);
		}

		m_DirtyAssetsReason = DirtyAssetsReason::None;
	}

	void EditorLayer::SaveDirtyAssets()
	{
		const size_t size = m_DirtyAssets.size();
		for (size_t i = 0; i < size; ++i)
		{
			const bool bSave = m_DirtyAssetsChecked[i];
			if (bSave == false)
				continue;

			const auto& asset = m_DirtyAssets[i];
			if (asset->GetAssetType() == AssetType::Scene)
			{
				if (m_OpenedSceneAsset == asset)
				{
					SaveScene();
					asset->SetDirty(false);
				}
			}
			else
				Asset::Save(asset);
		}
	}

	void EditorLayer::PrepareDirtyAssets(DirtyAssetsReason reason)
	{
		m_DirtyAssetsChecked.clear();
		m_DirtyAssets = AssetManager::GetDirtyAssets();
		m_DirtyAssetsChecked.resize(m_DirtyAssets.size(), true);
		m_ShowDirtyAssetMessage = m_DirtyAssets.empty() == false;
		m_DirtyAssetsReason = reason;
	}

	void EditorLayer::OpenProjectSelector()
	{
		Application::Get().CallNextFrame([thisLayer = shared_from_this()]()
		{
			Application& app = Application::Get();
			app.PopLayer(thisLayer);
			app.PushLayer(MakeRef<ProjectLayer>());
		});

		Project::Close();
	}

	void EditorLayer::Submit(const std::function<void()>& func)
	{
		std::scoped_lock lock(s_DeferredCallsMutex);
		m_DeferredCalls.push_back(func);
	}

	void EditorLayer::PlayScene()
	{
		if (m_EditorState != EditorState::Edit)
			return;

		m_EditorState = EditorState::Play;

		// Save some renderer settings
		{
			const auto& sceneRenderer = m_EditorScene->GetSceneRenderer();
			m_BeforeSimulationData.RendererSettings = sceneRenderer->GetOptions();
			m_BeforeSimulationData.Cubemap = m_EditorScene->GetSkybox();
			m_BeforeSimulationData.Sky = m_EditorScene->GetSkySettings();
			m_BeforeSimulationData.CubemapIntensity = m_EditorScene->GetSkyboxIntensity();
			m_BeforeSimulationData.bSkyAsBackground = m_EditorScene->GetUseSkyAsBackground();
			m_BeforeSimulationData.bSkyboxEnabled = m_EditorScene->IsSkyboxEnabled();
			m_BeforeSimulationData.bRenderSkybox = m_EditorScene->IsRenderSkyboxEnabled();
		}

		EG_CORE_TRACE("Editor Play pressed");
		m_SimulationScene = MakeRef<Scene>(m_EditorScene, "Simulation Scene");
		SetCurrentScene(m_SimulationScene);
		m_SimulationScene->OnRuntimeStart();
		m_PlaySound->Play();
	}

	void EditorLayer::StopPlayingScene()
	{
		if (m_EditorState == EditorState::Edit)
			return;

		EG_CORE_TRACE("Editor Stop pressed");
		m_SimulationScene->OnRuntimeStop();
		m_EditorState = EditorState::Edit;
		m_SimulationScene.reset();
		AssetManager::ResetRuntimeAsset();
		SetCurrentScene(m_EditorScene);

		// Restore some renderer settings
		{
			auto& sceneRenderer = m_EditorScene->GetSceneRenderer();
			sceneRenderer->SetOptions(m_BeforeSimulationData.RendererSettings);
			m_CurrentScene->SetSkybox(m_BeforeSimulationData.Cubemap);
			m_CurrentScene->SetSkybox(m_BeforeSimulationData.Sky);
			m_CurrentScene->SetSkyboxIntensity(m_BeforeSimulationData.CubemapIntensity);
			m_CurrentScene->SetUseSkyAsBackground(m_BeforeSimulationData.bSkyAsBackground);
			m_CurrentScene->SetSkyboxEnabled(m_BeforeSimulationData.bSkyboxEnabled);
			m_CurrentScene->SetRenderSkybox(m_BeforeSimulationData.bRenderSkybox);
		}
		Input::SetShowMouse(true); // Just in case restore the mouse state.
	}

	void EditorLayer::HandleOnSimulationButton()
	{
		// To restore selection settings
		auto selectedComp = m_SceneHierarchyPanel.GetSelectedComponentType();
		GUID selectedEntityGUID(0, 0);
		if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity())
			selectedEntityGUID = selectedEntity.GetGUID();

		if (m_EditorState == EditorState::Edit)
			PlayScene();
		else if (m_EditorState != EditorState::Edit)
			StopPlayingScene();

		if (!selectedEntityGUID.IsNull())
		{
			Entity selectedEntity = m_CurrentScene->GetEntityByGUID(selectedEntityGUID);
			m_SceneHierarchyPanel.SetEntitySelected(selectedEntity, selectedComp);
		}
	}

	void EditorLayer::HandleCloseRequest(bool bCloseEngine)
	{
		m_CloseEngineRequested = bCloseEngine;
		PrepareDirtyAssets(DirtyAssetsReason::ProjectClose);
		if (!m_ShowDirtyAssetMessage)
		{
			if (m_CloseEngineRequested)
				Application::Get().SetShouldClose(true);
			else
				OpenProjectSelector();
		}
	}

	const Ref<Image>& EditorLayer::GetRequiredGBufferImage(const Ref<SceneRenderer>& renderer, const GBuffer& gbuffer)
	{
		switch (m_VisualizingGBufferType)
		{
			case Eagle::EditorLayer::GBufferVisualizingType::Final: return renderer->GetOutput();
			case Eagle::EditorLayer::GBufferVisualizingType::Albedo: return gbuffer.Albedo;
			case Eagle::EditorLayer::GBufferVisualizingType::Emissive:  return gbuffer.Emissive;
			case Eagle::EditorLayer::GBufferVisualizingType::SSAO:  return renderer->GetSSAOResult();
			case Eagle::EditorLayer::GBufferVisualizingType::GTAO:  return renderer->GetGTAOResult();
			case Eagle::EditorLayer::GBufferVisualizingType::Motion: return gbuffer.Motion ? gbuffer.Motion : renderer->GetOutput();
			default: return renderer->GetOutput();
		}
	}

	void EditorLayer::SetVisualizingBufferType(GBufferVisualizingType value)
	{
		EG_CORE_TRACE("Visualizing GPU Buffer: {}", magic_enum::enum_name(value));
		m_VisualizingGBufferType = value;
	}

	void EditorLayer::BeginDocking()
	{
		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoCloseButton;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = (m_bFullScreen ? 0u : ImGuiWindowFlags_MenuBar) | ImGuiWindowFlags_NoDocking;
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
		ImGui::Begin("Eagle_DockspaceWindow", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("Eagle_Dockspace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
	}

	void EditorLayer::EndDocking()
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
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

		ImGui::Text("For help, go to the");
		ImGui::SameLine();
		UI::TextLink("github repository", "https://github.com/iceluna/eagle");
		ImGui::SameLine();
		ImGui::Text("where you can read the documentation or open an issue.");
		ImGui::Separator();

		if (ImGui::TreeNodeEx("Shortcuts", flags, "Shortcuts"))
		{
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("F5. Reloads the shaders if there were any changes.");
			ImGui::BulletText("Ctrl+N. Opens a new scene.");
			ImGui::BulletText("Ctrl+S. Saves the scene.");
			ImGui::BulletText("Ctrl+Shift+S. Opens up a dialogue to choose where to save the scene.");
			ImGui::BulletText("G. Toggles visibility of editor specific rendered elements (such as grid).");
			ImGui::BulletText("Alt+P. Toggles the simulation button.");
			ImGui::BulletText("Esc. Stops the simulation.");
			ImGui::BulletText("F11. Toggles viewport fullscreen mode.");
			ImGui::BulletText("Shift+F11. Toggles window fullscreen mode.");
			ImGui::BulletText("Q/W/E/R. Hidden/Location/Rotation/Scale gizmo modes.");
			ImGui::TreePop();
		}
		ImGui::Separator();

		if (ImGui::TreeNodeEx("Third party", flags, "Third party"))
		{
			ImGui::SetWindowFontScale(1.2f);
			UI::BulletLink("assimp", "https://github.com/assimp/assimp");
			UI::BulletLink("EnTT", "https://github.com/skypjack/entt");
			UI::BulletLink("FMOD. FMOD Studio. Firelight Technologies Pty Ltd", "https://www.fmod.com/");
			UI::BulletLink("GLFW", "https://www.glfw.org/");
			UI::BulletLink("glm", "https://github.com/g-truc/glm");
			UI::BulletLink("ImGui", "https://github.com/ocornut/imgui");
			UI::BulletLink("ImGuizmo", "https://github.com/CedricGuillemet/ImGuizmo");
			UI::BulletLink("magic_enum", "https://github.com/Neargye/magic_enum");
			UI::BulletLink("mono", "https://github.com/mono/mono");
			UI::BulletLink("MSDF atlas generator", "https://github.com/Chlumsky/msdf-atlas-gen");
			UI::BulletLink("PhysX", "https://github.com/NVIDIAGameWorks/PhysX");
			UI::BulletLink("spdlog", "https://github.com/gabime/spdlog");
			UI::BulletLink("stb_image", "https://github.com/nothings/stb");
			UI::BulletLink("Thread pool", "https://github.com/bshoshany/thread-pool");
			UI::BulletLink("Vulkan memory allocator", "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator");
			UI::BulletLink("yaml-cpp", "https://github.com/jbeder/yaml-cpp");
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
