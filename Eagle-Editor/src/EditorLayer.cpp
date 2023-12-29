#include "EditorLayer.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Core/SceneSerializer.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Debug/CPUTimings.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo.h>
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

	static std::mutex s_DeferredCallsMutex;
	
	static glm::vec3 notUsed1;
	static glm::vec4 notUsed2;

	static void BeginDocking();
	static void EndDocking();
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
		, m_SceneHierarchyPanel(*this)
		, m_EditorSerializer(this)
		, m_ContentBrowserPanel(*this)
		, m_Window(Application::Get().GetWindow())
	{}

	void EditorLayer::OnAttach()
	{
		ScriptEngine::LoadAppAssembly("Sandbox.dll");
		m_GuizmoType = ImGuizmo::OPERATION::TRANSLATE;

		m_WindowTitle = m_Window.GetWindowTitle();
		Scene::AddOnSceneOpenedCallback(m_OpenedSceneCallbackID, [this](const Ref<Scene>& scene)
		{
			if (m_EditorState == EditorState::Edit)
			{
				m_EditorScene = scene;
				m_OpenedScenePath = m_EditorScene->GetDebugName();
				UpdateEditorTitle(m_OpenedScenePath.empty() ? "Untitled.eagle" : m_OpenedScenePath.u8string());
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

			EG_CORE_INFO("Opened a scene: {}", m_OpenedScenePath.empty() ? "<New Scene>" : scene->GetDebugName());
		});

		// If failed to deserialize, create EditorDefault.ini & open a new scene
		if (m_EditorSerializer.Deserialize("../Sandbox/Engine/EditorDefault.ini") == false)
		{
			m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
			NewScene();
		}
	
		SoundSettings soundSettings;
		soundSettings.Volume = 0.25f;
		m_PlaySound = Sound2D::Create("assets/audio/playsound.wav", soundSettings);

		m_PlayButtonIcon = Texture2D::Create("assets/textures/Editor/playbutton.png");
		m_StopButtonIcon = Texture2D::Create("assets/textures/Editor/stopbutton.png");
	}

	void EditorLayer::OnDetach()
	{
		m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
		Scene::SetCurrentScene(nullptr);
		Scene::RemoveOnSceneOpenedCallback(m_OpenedSceneCallbackID);
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		EG_CPU_TIMING_SCOPED("EditorLayer. OnUpdate");

		m_Ts = ts;

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
		m_CurrentScene->OnUpdate(ts, !m_ViewportHidden && bShouldRenderBasedOnFocus);
		HandleEntitySelection();
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

		if (e.GetEventType() == EventType::WindowFocused)
			m_WindowFocused = ((WindowFocusedEvent&)e).IsFocused();

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
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
			DrawSettings();
			DrawEditorPreferences();
			DrawStats();
		}

		DrawViewport();
		
		if (!m_bFullScreen)
		{
			DrawSimulatePanel();
			m_SceneHierarchyPanel.OnImGuiRender();
			m_ContentBrowserPanel.OnImGuiRender();
			m_ConsolePanel.OnImGuiRender();
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
					ShaderLibrary::ReloadAllShaders();
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
					m_CurrentScene->bDrawMiscellaneous = !m_CurrentScene->bDrawMiscellaneous;
				break;

			case Key::P:
				if (leftAlt)
					HandleOnSimulationButton();
				break;

			case Key::F11:
			{
				if (leftShift)
				{
					Window& window = Application::Get().GetWindow();
					window.SetFullscreen(!window.IsFullscreen());
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

	void EditorLayer::ReloadScriptsIfNecessary()
	{
		static bool bRequiresScriptsRebuild = false;
		if (bRequiresScriptsRebuild || Utils::WereScriptsRebuild())
		{
			if (m_EditorState == EditorState::Edit)
			{
				bRequiresScriptsRebuild = false;
				ScriptEngine::LoadAppAssembly("Sandbox.dll");
			}
			else bRequiresScriptsRebuild = true; // Set it to true since it might be false and `Utils::WereScriptsRebuild()` is triggered only once
		}
	}

	void EditorLayer::HandleResize()
	{
		if (m_NewViewportSize != m_CurrentViewportSize)
		{
			m_CurrentViewportSize = m_NewViewportSize;
			m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
			if (m_SimulationScene)
				m_SimulationScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
		}
	}

	void EditorLayer::HandleEntitySelection()
	{
		if (m_EditorState == EditorState::Play)
			return;

		//Entity Selection
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		bool bUsingImGuizmo = selectedEntity && (ImGuizmo::IsUsing() || ImGuizmo::IsOver());
		if (m_ViewportHovered && !bUsingImGuizmo && Input::IsMouseButtonPressed(Mouse::ButtonLeft))
		{
			auto [mx, my] = ImGui::GetMousePos();
			mx -= m_ViewportBounds[0].x;
			my -= m_ViewportBounds[0].y;

			const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
			{
				Ref<Image>& image = m_CurrentScene->GetSceneRenderer()->GetGBuffer().ObjectIDCopy;
				int data = -1;

				const ImageSubresourceLayout imageLayout = image->GetImageSubresourceLayout();
				uint8_t* mapped = (uint8_t*)image->Map();
				mapped += imageLayout.Offset;
				mapped += imageLayout.RowPitch * mouseY;
				memcpy(&data, ((uint32_t*)mapped) + mouseX, sizeof(int));
				image->Unmap();
				m_SceneHierarchyPanel.SetEntitySelected(data);
			}
		}
	}

	void EditorLayer::NewScene()
	{
		if (m_EditorState == EditorState::Edit)
			Scene::OpenScene("");
	}

	void EditorLayer::OpenScene(const Path& filepath)
	{
		if (m_EditorState == EditorState::Edit)
			Scene::OpenScene(filepath);
	}

	// TODO: Don't use file dialog because user can save it outside of Content folder
	bool EditorLayer::SaveScene()
	{
		if (m_EditorState != EditorState::Edit)
			return false;

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
				return false;
			}
		}
		else
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(m_OpenedScenePath);
		}
		return true;
	}

	bool EditorLayer::SaveSceneAs()
	{
		if (m_EditorState != EditorState::Edit)
			return false;

		std::filesystem::path filepath = FileDialog::SaveFile(FileDialog::SCENE_FILTER);
		if (!filepath.empty())
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(filepath);

			m_OpenedScenePath = filepath; UpdateEditorTitle(m_OpenedScenePath);
			return true;
		}

		EG_CORE_ERROR("Couldn't save scene {0}", filepath);
		return false;
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

	void EditorLayer::OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, const SceneRendererSettings& settings, bool bWindowMaximized, bool bVSync, bool bRenderOnlyWhenFocused, Key stopSimulationKey)
	{
		// Scene creation needs to go through this way of setting it up since we need to get Ref<Scene> immediately
		m_EditorScene = MakeRef<Scene>("Editor Scene");
		SetCurrentScene(m_EditorScene);
		if (std::filesystem::exists(m_OpenedScenePath))
		{
			SceneSerializer ser(m_EditorScene);
			ser.Deserialize(m_OpenedScenePath);
			UpdateEditorTitle(m_OpenedScenePath);
		}
		else
		{
			m_OpenedScenePath = "";
			UpdateEditorTitle("Untitled.eagle");
		}
		m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);

		Window& window = Application::Get().GetWindow();
		window.SetVSync(bVSync);
		ImGuiLayer::SelectStyle(m_EditorStyle);
		this->bRenderOnlyWhenFocused = bRenderOnlyWhenFocused;
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
		auto options = sceneRenderer->GetOptions();
		options.BloomSettings = settings.BloomSettings;
		options.SSAOSettings = settings.SSAOSettings;
		options.GTAOSettings = settings.GTAOSettings;
		options.FogSettings = settings.FogSettings;
		options.ShadowsSettings = settings.ShadowsSettings;
		options.VolumetricSettings = settings.VolumetricSettings;
		options.bEnableSoftShadows = settings.bEnableSoftShadows;
		options.bTranslucentShadows = settings.bTranslucentShadows;
		options.bEnableCSMSmoothTransition = settings.bEnableCSMSmoothTransition;
		options.bStutterlessShaders = settings.bStutterlessShaders;
		options.bEnableObjectPicking = settings.bEnableObjectPicking;
		options.bEnable2DObjectPicking = settings.bEnable2DObjectPicking;
		options.LineWidth = settings.LineWidth;
		options.GridScale = settings.GridScale;
		options.TransparencyLayers = settings.TransparencyLayers;
		options.AO = settings.AO;
		options.AA = settings.AA;
		options.Gamma = settings.Gamma;
		options.Exposure = settings.Exposure;
		options.Tonemapping = settings.Tonemapping;
		options.PhotoLinearTonemappingParams = settings.PhotoLinearTonemappingParams;
		options.FilmicTonemappingParams = settings.FilmicTonemappingParams;
		sceneRenderer->SetOptions(options);
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

		if (selectedEntity && (m_GuizmoType != -1))
		{
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
			//Entity transform
			if (selectedComponent)
				transform = selectedComponent->GetWorldTransform();
			else
				transform = selectedEntity.GetWorldTransform();

			int snappingIndex = 0;
			if (m_GuizmoType == ImGuizmo::OPERATION::ROTATE)
				snappingIndex = 1;
			else if (m_GuizmoType == ImGuizmo::OPERATION::SCALE)
				snappingIndex = 2;

			//Snapping
			const float snapValues[3] = { m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex], m_SnappingValues[snappingIndex] };
			const bool bSnap = Input::IsKeyPressed(Key::LeftShift);

			ImGuizmo::SetID(int(selectedEntity.GetID()));

			glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);
			ImGuizmo::Manipulate(glm::value_ptr(cameraViewMatrix), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GuizmoType,
				ImGuizmo::WORLD, glm::value_ptr(transformMatrix), nullptr, bSnap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
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
					Eagle::Application::Get().SetShouldClose(true);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Windows"))
			{
				bool bConsoleOpened = m_ConsolePanel.IsOpened();
				if (ImGui::Checkbox("Console", &bConsoleOpened))
					m_ConsolePanel.SetOpened(bConsoleOpened);

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

				UI::Text("Pass name", "Time (ms)");
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
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

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
					float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
					ImGui::Separator();
					bool treeOpened = ImGui::TreeNodeEx("CPU Timings", flags, threadName.data());
					ImGui::PopStyleVar();
					if (treeOpened)
					{
						UI::BeginPropertyGrid("CPUTimings");

						UI::Text("Name", "Time (ms)");
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
		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();

		ImGui::PushID("SceneSettings");
		ImGui::Begin("Scene Settings");

		constexpr uint64_t treeID1 = 95292191ull;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		bool treeOpened = ImGui::TreeNodeEx((void*)treeID1, flags, "Skybox Settings");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("IBLSceneSettings");

			auto cubemap = sceneRenderer->GetSkybox();
			if (UI::DrawTextureCubeSelection("IBL", cubemap))
				sceneRenderer->SetSkybox(cubemap);
			
			float iblIntensity = sceneRenderer->GetSkyboxIntensity();
			if (UI::PropertyDrag("IBL Lighting Intensity", iblIntensity, 0.1f))
				sceneRenderer->SetSkyboxIntensity(iblIntensity);

			ImGui::Separator();

			auto skySettings = sceneRenderer->GetSkySettings();
			bool bChanged = false;
			int cumulusLayers = skySettings.CumulusLayers;

			bChanged |= UI::PropertyDrag("Sky Sun Position", skySettings.SunPos, 0.01f);
			bChanged |= UI::PropertyDrag("Sky Intensity", skySettings.SkyIntensity, 0.1f);
			bChanged |= UI::PropertyDrag("Sky Scattering", skySettings.Scattering, 0.01f, 0.001f, 0.999f);

			bChanged |= UI::Property("Cirrus Clouds", skySettings.bEnableCirrusClouds);
			bChanged |= UI::Property("Cumulus Clouds", skySettings.bEnableCumulusClouds);

			bChanged |= UI::PropertyColor("Clouds Color", skySettings.CloudsColor);
			bChanged |= UI::PropertyDrag("Clouds Intensity", skySettings.CloudsIntensity, 0.1f);

			bChanged |= UI::PropertyDrag("Cirrus Clouds Amount", skySettings.Cirrus, 0.01f);
			bChanged |= UI::PropertyDrag("Cumulus Clouds Amount", skySettings.Cumulus, 0.01f);
			if (UI::PropertyDrag("Cumulus Clouds Layers", cumulusLayers, 1.f, 1, INT_MAX))
			{
				skySettings.CumulusLayers = uint32_t(cumulusLayers);
				bChanged = true;
			}

			if (bChanged)
				sceneRenderer->SetSkybox(skySettings);

			ImGui::Separator();
			bool bUseSkyAsBackground = sceneRenderer->GetUseSkyAsBackground();
			if (UI::Property("Sky as background", bUseSkyAsBackground, s_SkyHelpMsg))
				sceneRenderer->SetUseSkyAsBackground(bUseSkyAsBackground);

			bool bEnableSkybox = sceneRenderer->IsSkyboxEnabled();
			if (UI::Property("Enable Skybox", bEnableSkybox, s_SkyboxEnableHelpMsg))
				sceneRenderer->SetSkyboxEnabled(bEnableSkybox);

			UI::EndPropertyGrid();
			ImGui::TreePop();
		}

		ImGui::End();
		ImGui::PopID();
	}

	void EditorLayer::DrawSettings()
	{
		ImGui::Begin("Renderer Settings");
		UI::BeginPropertyGrid("RendererSettingsPanel");

		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		SceneRendererSettings options = sceneRenderer->GetOptions();
		bool bSettingsChanged = false;

		bool bVSync = Application::Get().GetWindow().IsVSync();
		if (UI::Property("VSync", bVSync))
		{
			EG_EDITOR_TRACE("Changed VSync to: {}", bVSync);
			Application::Get().GetWindow().SetVSync(bVSync);
		}

		bSettingsChanged |= UI::PropertyDrag("Gamma", options.Gamma, 0.1f, 0.0f, 10.f);
		bSettingsChanged |= UI::PropertyDrag("Exposure", options.Exposure, 0.1f, 0.0f, 100.f);
		bSettingsChanged |= UI::ComboEnum<TonemappingMethod>("Tonemapping", options.Tonemapping);

		if (UI::Property("Stutterless", options.bStutterlessShaders, s_StutterlessHelpMsg))
		{
			EG_EDITOR_TRACE("Changed Stutterless to: {}", options.bStutterlessShaders);
			bSettingsChanged = true;
		}

		if (UI::Property("In-game object picking", options.bEnableObjectPicking, "You can disable it through C# when it's not needed to improve performance and reduce memory usage"))
		{
			EG_EDITOR_TRACE("Changed Object Picking to: {}", options.bEnableObjectPicking);
			bSettingsChanged = true;
		}

		if (UI::Property("In-game 2D object picking", options.bEnable2DObjectPicking, "If set to false, 2D objects will be ignored (for example, Text2D and Image2D components)"))
		{
			EG_EDITOR_TRACE("Changed 2D Object Picking to: {}", options.bEnable2DObjectPicking);
			bSettingsChanged = true;
		}

		if (UI::PropertyDrag("Line width", options.LineWidth, 0.1f))
		{
			options.LineWidth = glm::max(options.LineWidth, 0.f);
			bSettingsChanged = true;
			EG_EDITOR_TRACE("Changed Line Width to: {}", options.LineWidth);
		}

		if (UI::PropertyDrag("Grad Scale", options.GridScale, 0.1f))
		{
			options.GridScale = glm::max(options.GridScale, 0.f);
			bSettingsChanged = true;
			EG_EDITOR_TRACE("Changed Grad Scale to: {}", options.GridScale);
		}

		int transparencyLayers = options.TransparencyLayers;
		if (UI::PropertyDrag("Transparency Layers", transparencyLayers, 1.f, 1, 16, s_TransparencyLayersHelpMsg))
		{
			options.TransparencyLayers = uint32_t(transparencyLayers);
			bSettingsChanged = true;
			EG_EDITOR_TRACE("Changed Transparency Layers to: {}", options.TransparencyLayers);
		}

		// Ambient Occlusion method
		{
			const AmbientOcclusion oldAO = options.AO;
			if (UI::ComboEnum<AmbientOcclusion>("Ambient Occlusion", options.AO))
			{
				bSettingsChanged = true;
				EG_EDITOR_TRACE("Changed AO to: {}", magic_enum::enum_name(options.AO));

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
			EG_EDITOR_TRACE("Changed AA to: {}", magic_enum::enum_name(options.AA));
		}

		UI::EndPropertyGrid();

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		// Shadow Resolutions settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Shadow Settings", treeFlags);
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
					EG_EDITOR_TRACE("Point Light ShadowMap Size changed to: {}", settings.PointLightShadowMapSize);
					bSettingsChanged = true;
				}

				if (UI::PropertyDrag("Spot Light ShadowMap Size", settings.SpotLightShadowMapSize, 32.f, 0, 16384))
				{
					settings.SpotLightShadowMapSize = glm::max(settings.SpotLightShadowMapSize, ShadowMapsSettings::MinSpotLightShadowMapSize);
					EG_EDITOR_TRACE("Spot Light ShadowMap Size changed to: {}", settings.SpotLightShadowMapSize);
					bSettingsChanged = true;
				}

				ImGui::Separator();

				for (uint32_t i = 0; i < RendererConfig::CascadesCount; ++i)
				{
					const std::string name = std::string("Dir Light ShadowMap Size #") + std::to_string(i + 1);
					if (UI::PropertyDrag(name, settings.DirLightShadowMapSizes[i], 32.f, 0, 16384))
					{
						settings.DirLightShadowMapSizes[i] = glm::max(settings.DirLightShadowMapSizes[i], ShadowMapsSettings::MinDirLightShadowMapSize);
						EG_EDITOR_TRACE("{} changed to: {}", name, settings.DirLightShadowMapSizes[i]);
						bSettingsChanged = true;
					}
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Bloom settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Bloom Settings", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Bloom Settings");

				BloomSettings& settings = options.BloomSettings;
				if (UI::Property("Enable Bloom", settings.bEnable))
				{
					EG_EDITOR_TRACE("Enabled Bloom: {}", settings.bEnable);
					bSettingsChanged = true;
				}

				if (UI::PropertyDrag("Threshold", settings.Threshold, 0.05f))
				{
					settings.Threshold = std::max(0.f, settings.Threshold);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Bloom Threshold to: {}", settings.Threshold);
				}
				if (UI::PropertyDrag("Intensity", settings.Intensity, 0.05f))
				{
					settings.Intensity = std::max(0.f, settings.Intensity);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Bloom Intensity to: {}", settings.Intensity);
				}
				if (UI::PropertyDrag("Dirt Intensity", settings.DirtIntensity, 0.05f))
				{
					settings.DirtIntensity = std::max(0.f, settings.DirtIntensity);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Bloom Dirt Intensity to: {}", settings.DirtIntensity);
				}
				if (UI::PropertyDrag("Knee", settings.Knee, 0.01f))
				{
					settings.Knee = std::max(0.f, settings.Knee);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Bloom Knee to: {}", settings.Knee);
				}
				if (UI::DrawTexture2DSelection("Dirt", settings.Dirt))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Bloom Dirt Texture to: {}", settings.Dirt ? settings.Dirt->GetPath() : "None");
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}
		
		// SSAO settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("SSAO Settings", treeFlags);
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
					EG_EDITOR_TRACE("Changed SSAO Samples Number to: {}", settings.GetNumberOfSamples());
				}
				if (UI::PropertyDrag("Radius", radius, 0.01f))
				{
					settings.SetRadius(radius);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed SSAO Radius to: {}", settings.GetRadius());
				}
				if (UI::PropertyDrag("Bias", bias, 0.01f))
				{
					settings.SetBias(bias);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed SSAO Bias to: {}", settings.GetBias());
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// GTAO settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("GTAO Settings", treeFlags);
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
					EG_EDITOR_TRACE("Changed GTAO Samples Number to: {}", settings.GetNumberOfSamples());
				}
				if (UI::PropertyDrag("Radius", radius, 0.01f))
				{
					settings.SetRadius(radius);
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed GTAO Radius to: {}", settings.GetRadius());
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Volumetric settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Volumetric Lights Settings", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Volumetric Lights Settings");

				VolumetricLightsSettings& settings = options.VolumetricSettings;
				if (UI::Property("Enable Volumetric Lights", settings.bEnable, s_EnableVolumetricLightsHelpMsg))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Enabled Volumetric Lights: {}", settings.bEnable);
				}

				if (UI::Property("Enable Volumetric Fog", settings.bFogEnable))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Enabled Volumetric Fog: {}", settings.bFogEnable);
				}

				if (UI::PropertyDrag("Samples", settings.Samples, 1.f, 1, 0, "Use with caution! Making it to high might kill the performance. Especially if the light casts shadows"))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Volumetric Samples to: {}", settings.Samples);
				}
				if (UI::PropertyDrag("Max Scattering Distance", settings.MaxScatteringDistance, 1.f, 0.f, FLT_MAX))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Volumetric Max Scattering Distance to: {}", settings.MaxScatteringDistance);
				}
				if (UI::PropertyDrag("Fog Speed", settings.FogSpeed, 0.01f))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Changed Volumetric Fog Speed to: {}", settings.FogSpeed);
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}

		// Fog settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Fog Settings", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("Fog Settings");

				FogSettings& settings = options.FogSettings;
				if (UI::Property("Enable Fog", settings.bEnable))
				{
					bSettingsChanged = true;
					EG_EDITOR_TRACE("Enabled Fog: {}", settings.bEnable);
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

		// Photo Linear Tonemapping Settings
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("Photo Linear Tonemapping Settings", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("PhotoSettings");

				auto params = options.PhotoLinearTonemappingParams;
				bool bChanged = false;
				if (UI::PropertyDrag("Sensitivity", params.Sensitivity, 0.01f))
				{
					EG_EDITOR_TRACE("Changed Photo Linear sensitivity to: {}", params.Sensitivity);
					bChanged = true;
				}
				if (UI::PropertyDrag("Exposure time (sec)", params.ExposureTime, 0.01f))
				{
					EG_EDITOR_TRACE("Changed Photo Linear Exposure time to: {}", params.ExposureTime);
					bChanged = true;
				}
				if (UI::PropertyDrag("F-Stop", params.FStop, 0.01f))
				{
					EG_EDITOR_TRACE("Changed Photo Linear F-Stop to: {}", params.FStop);
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
			bool treeOpened = ImGui::TreeNodeEx("Filmic Tonemapping Settings", treeFlags);
			ImGui::PopStyleVar();

			if (treeOpened)
			{
				UI::BeginPropertyGrid("FilmicSettings");

				if (UI::PropertyDrag("White Point", options.FilmicTonemappingParams.WhitePoint, 0.05f))
				{
					EG_EDITOR_TRACE("Changed Filmic White Point to: {}", options.FilmicTonemappingParams.WhitePoint);
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
	
	void EditorLayer::DrawEditorPreferences()
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

			UI::Property("Eco rendering", bRenderOnlyWhenFocused, "If checked, the scene won't render if the window is not in focus");
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
				| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

			bool renderer3DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer3D", flags, "Renderer3D Stats");
			if (renderer3DTreeOpened)
			{
				auto stats = m_CurrentScene->GetSceneRenderer()->GetStats();

				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Vertices: %d", stats.Vertices);
				ImGui::Text("Indices: %d", stats.Indeces);

				ImGui::TreePop();
			}

			bool renderer2DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer2D", flags, "Renderer2D Stats");
			if (renderer2DTreeOpened)
			{
				auto stats = m_CurrentScene->GetSceneRenderer()->GetStats2D();

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
				m_CurrentScene->ViewportBounds[0] = m_ViewportBounds[0];
				m_CurrentScene->ViewportBounds[1] = m_ViewportBounds[1];

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

				UI::Image(*m_ViewportImage, ImVec2{ m_CurrentViewportSize.x, m_CurrentViewportSize.y });

				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
					ImGui::SetWindowFocus();
			}
		}

		//---------------------------Gizmos---------------------------
		{
			if (m_EditorState == EditorState::Edit)
				UpdateGuizmo();

			ImGui::End(); //Viewport
			ImGui::PopStyleVar();
		}
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
			m_BeforeSimulationData.Cubemap = sceneRenderer->GetSkybox();
			m_BeforeSimulationData.Sky = sceneRenderer->GetSkySettings();
			m_BeforeSimulationData.CubemapIntensity = sceneRenderer->GetSkyboxIntensity();
			m_BeforeSimulationData.bSkyAsBackground = sceneRenderer->GetUseSkyAsBackground();
			m_BeforeSimulationData.bSkyboxEnabled = sceneRenderer->IsSkyboxEnabled();
		}

		m_SimulationScene = MakeRef<Scene>(m_EditorScene, "Simulation Scene");
		SetCurrentScene(m_SimulationScene);
		m_SimulationScene->OnRuntimeStart();
		m_PlaySound->Play();
		EG_EDITOR_TRACE("Editor Play pressed");
	}

	void EditorLayer::StopPlayingScene()
	{
		if (m_EditorState == EditorState::Edit)
			return;

		m_SimulationScene->OnRuntimeStop();
		m_EditorState = EditorState::Edit;
		m_SimulationScene.reset();
		SetCurrentScene(m_EditorScene);

		// Restore some renderer settings
		{
			auto& sceneRenderer = m_EditorScene->GetSceneRenderer();
			sceneRenderer->SetOptions(m_BeforeSimulationData.RendererSettings);
			sceneRenderer->SetSkybox(m_BeforeSimulationData.Cubemap);
			sceneRenderer->SetSkybox(m_BeforeSimulationData.Sky);
			sceneRenderer->SetSkyboxIntensity(m_BeforeSimulationData.CubemapIntensity);
			sceneRenderer->SetUseSkyAsBackground(m_BeforeSimulationData.bSkyAsBackground);
			sceneRenderer->SetSkyboxEnabled(m_BeforeSimulationData.bSkyboxEnabled);
		}

		EG_EDITOR_TRACE("Editor Stop pressed");
	}

	void EditorLayer::HandleOnSimulationButton()
	{
		if (m_EditorState == EditorState::Edit)
			PlayScene();
		else if (m_EditorState != EditorState::Edit)
			StopPlayingScene();
	}

	const Ref<Image>& EditorLayer::GetRequiredGBufferImage(const Ref<SceneRenderer>& renderer, const GBuffer& gbuffer)
	{
		switch (m_VisualizingGBufferType)
		{
			case Eagle::EditorLayer::GBufferVisualizingType::Final: return renderer->GetOutput();
			case Eagle::EditorLayer::GBufferVisualizingType::Albedo: return gbuffer.AlbedoRoughness;
			case Eagle::EditorLayer::GBufferVisualizingType::Emissive:  return gbuffer.Emissive;
			case Eagle::EditorLayer::GBufferVisualizingType::SSAO:  return renderer->GetSSAOResult();
			case Eagle::EditorLayer::GBufferVisualizingType::GTAO:  return renderer->GetGTAOResult();
			case Eagle::EditorLayer::GBufferVisualizingType::Motion: return gbuffer.Motion ? gbuffer.Motion : renderer->GetOutput();
			default: return renderer->GetOutput();
		}
	}

	void EditorLayer::SetVisualizingBufferType(GBufferVisualizingType value)
	{
		EG_EDITOR_TRACE("Visualizing GPU Buffer: {}", magic_enum::enum_name(value));
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
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

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
