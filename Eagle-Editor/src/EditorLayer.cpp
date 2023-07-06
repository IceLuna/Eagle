#include "EditorLayer.h"

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
	static std::mutex s_DeferredCallsMutex;

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

		// If failed to deserialize, create EditorDefault.ini & open a new scene
		if (m_EditorSerializer.Deserialize("../Sandbox/Engine/EditorDefault.ini") == false)
		{
			m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
			NewScene(true);
		}
	
		SoundSettings soundSettings;
		soundSettings.Volume = 0.25f;
		m_PlaySound = Sound2D::Create("assets/audio/playsound.wav", soundSettings);

		m_PlayButtonIcon = Texture2D::Create("assets/textures/Editor/playbutton.png", {}, false);
		m_StopButtonIcon = Texture2D::Create("assets/textures/Editor/stopbutton.png", {}, false);
	}

	void EditorLayer::OnDetach()
	{
		m_EditorSerializer.Serialize("../Sandbox/Engine/EditorDefault.ini");
		Scene::SetCurrentScene(nullptr);
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

		ReloadScriptsIfNecessary();
		HandleResize();
		m_CurrentScene->OnUpdate(ts, !m_ViewportHidden);
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

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(EditorLayer::OnKeyPressed));
	}

	void EditorLayer::OnImGuiRender()
	{
		EG_CPU_TIMING_SCOPED("EditorLayer. UI");

		BeginDocking();
		m_VSync = Application::Get().GetWindow().IsVSync();

		DrawMenuBar();
		DrawSceneSettings();
		DrawSettings();
		DrawEditorPreferences();
		DrawStats();
		DrawViewport();
		DrawSimulatePanel();

		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();
		m_ConsolePanel.OnImGuiRender();

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

			case Key::G:
				if (m_ViewportFocused)
					m_CurrentScene->bDrawMiscellaneous = !m_CurrentScene->bDrawMiscellaneous;
				break;
		}

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
		}
	}

	void EditorLayer::HandleEntitySelection()
	{
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
				RenderManager::Submit([editorLayer = this, mouseX, mouseY](Ref<CommandBuffer>&)
				{
					GBuffer& gBuffer = editorLayer->m_CurrentScene->GetSceneRenderer()->GetGBuffer();
					int data = -1;
					gBuffer.ObjectID->Read(&data, sizeof(int), glm::ivec3{ mouseX, mouseY, 0 }, glm::uvec3{ 1 }, ImageReadAccess::PixelShaderRead, ImageReadAccess::PixelShaderRead);
					editorLayer->Submit([editorLayer, data]()
					{
						editorLayer->m_SceneHierarchyPanel.SetEntitySelected(data);
					});
				});
			}
		}
	}

	void EditorLayer::NewScene(bool bImmediately)
	{
		if (m_EditorState == EditorState::Edit)
		{
			auto func = [this]()
			{
				ComponentsNotificationSystem::ResetSystem();
				ScriptEngine::Reset();
				RenderManager::Wait();
				m_EditorScene = MakeRef<Scene>();
				SetCurrentScene(m_EditorScene);
				m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);
				m_OpenedScenePath = "";
				UpdateEditorTitle("Untitled.eagle");
			};
			if (bImmediately)
				func();
			else
				Application::Get().CallNextFrame(func);
		}
	}

	void EditorLayer::OpenScene()
	{
		if (m_EditorState == EditorState::Edit)
		{
			std::filesystem::path filepath = FileDialog::OpenFile(FileDialog::SCENE_FILTER);
			OpenScene(filepath, false);
		}
	}

	void EditorLayer::OpenScene(const std::filesystem::path& filepath, bool bImmediately)
	{
		if (m_EditorState == EditorState::Edit)
		{
			auto func = [this, filepath]()
			{
				if (filepath.empty())
				{
					EG_CORE_ERROR("Failed to load scene. Path is null");
					return;
				}

				ComponentsNotificationSystem::ResetSystem();
				ScriptEngine::Reset();
				RenderManager::Wait();
				m_EditorScene = MakeRef<Scene>();
				SetCurrentScene(m_EditorScene);
				m_EditorScene->OnViewportResize((uint32_t)m_CurrentViewportSize.x, (uint32_t)m_CurrentViewportSize.y);

				SceneSerializer serializer(m_EditorScene);
				serializer.Deserialize(filepath);

				m_OpenedScenePath = filepath;
				UpdateEditorTitle(m_OpenedScenePath);
			};
			if (bImmediately)
				func();
			else
				Application::Get().CallNextFrame(func);
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

	void EditorLayer::OnDeserialized(const glm::vec2& windowSize, const glm::vec2& windowPos, const SceneRendererSettings& settings, bool bWindowMaximized)
	{
		if (std::filesystem::exists(m_OpenedScenePath))
		{
			m_EditorScene = MakeRef<Scene>();
			SetCurrentScene(m_EditorScene);
			SceneSerializer ser(m_EditorScene);
			if (ser.Deserialize(m_OpenedScenePath))
			{
				UpdateEditorTitle(m_OpenedScenePath);
			}
		}
		else
		{
			NewScene(true);
		}

		Window& window = Application::Get().GetWindow();
		window.SetVSync(m_VSync);
		ImGuiLayer::SelectStyle(m_EditorStyle);
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
		options.bEnableSoftShadows = settings.bEnableSoftShadows;
		options.bEnableCSMSmoothTransition = settings.bEnableCSMSmoothTransition;
		options.LineWidth = settings.LineWidth;
		options.GridScale = settings.GridScale;
		options.AO = settings.AO;
		sceneRenderer->SetOptions(options);
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
			glm::mat4 cameraProjection = bEditing ? editorCamera.GetProjection() : runtimeCamera->Camera.GetProjection();
			const glm::mat4& cameraViewMatrix = bEditing ? editorCamera.GetViewMatrix() : runtimeCamera->GetViewMatrix();
			cameraProjection[1][1] *= -1.f; // Since in Vulkan [1][1] of Projection is flipped, we need to flip it back for Guizmo

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

	void EditorLayer::DrawMenuBar()
	{
		static bool bShowGPUMemoryUsage = false;
		static bool bShowGPUTimings = false;
		static bool bShowCPUTimings = false;
		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		const GBuffer& gBuffers = sceneRenderer->GetGBuffer();
		m_ViewportImage = &(GetRequiredGBufferImage(sceneRenderer, gBuffers));

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					NewScene();
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
					int oldValue = m_SelectedBufferIndex;
					int radioButtonIndex = 0;

					if (ImGui::RadioButton("Albedo", &m_SelectedBufferIndex, radioButtonIndex++))
					{
						if (oldValue == m_SelectedBufferIndex)
						{
							m_SelectedBufferIndex = -1;
							SetVisualizingBufferType(GBufferVisualizingType::Final);
						}
						else
						{
							SetVisualizingBufferType(GBufferVisualizingType::Albedo);
						}
					}
					if (ImGui::RadioButton("Emission", &m_SelectedBufferIndex, radioButtonIndex++))
					{
						if (oldValue == m_SelectedBufferIndex)
						{
							m_SelectedBufferIndex = -1;
							SetVisualizingBufferType(GBufferVisualizingType::Final);
						}
						else
						{
							SetVisualizingBufferType(GBufferVisualizingType::Emissive);
						}
					}
					if (sceneRenderer->GetOptions().OptionalGBuffers.bMotion)
					{
						if (ImGui::RadioButton("Motion", &m_SelectedBufferIndex, radioButtonIndex++))
						{
							if (oldValue == m_SelectedBufferIndex)
							{
								m_SelectedBufferIndex = -1;
								SetVisualizingBufferType(GBufferVisualizingType::Final);
							}
							else
							{
								SetVisualizingBufferType(GBufferVisualizingType::Motion);
							}
						}
					}
					if (sceneRenderer->GetOptions().AO == AmbientOcclusion::SSAO)
					{
						if (ImGui::RadioButton("SSAO", &m_SelectedBufferIndex, radioButtonIndex++))
						{
							if (oldValue == m_SelectedBufferIndex)
							{
								m_SelectedBufferIndex = -1;
								SetVisualizingBufferType(GBufferVisualizingType::Final);
							}
							else
							{
								SetVisualizingBufferType(GBufferVisualizingType::SSAO);
							}
						}
					}
					if (sceneRenderer->GetOptions().AO == AmbientOcclusion::GTAO)
					{
						if (ImGui::RadioButton("GTAO", &m_SelectedBufferIndex, radioButtonIndex++))
						{
							if (oldValue == m_SelectedBufferIndex)
							{
								m_SelectedBufferIndex = -1;
								SetVisualizingBufferType(GBufferVisualizingType::Final);
							}
							else
							{
								SetVisualizingBufferType(GBufferVisualizingType::GTAO);
							}
						}
					}
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
				if (UI::Property("Visualize CSM", options.bVisualizeCascades, "Red, green, blue, purple"))
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

			UI::Text("Total usage (MBs): ", std::to_string(uint64_t(stats.Used * toMBs)));
			UI::Text("Free (MBs): ", std::to_string(uint64_t(stats.Free * toMBs)));

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
		ImGui::PushID("SceneSettings");
		ImGui::Begin("Scene Settings");
		constexpr uint64_t treeID1 = 95292191ull;
		constexpr uint64_t treeID2 = 95292192ull;
		constexpr uint64_t treeID3 = 95292193ull;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		SceneRendererSettings rendererOptions = sceneRenderer->GetOptions();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		bool treeOpened = ImGui::TreeNodeEx((void*)treeID1, flags, "Skybox");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("SkyboxSceneSettings");

			auto cubemap = sceneRenderer->GetSkybox();
			if (UI::DrawTextureCubeSelection("IBL", cubemap))
				sceneRenderer->SetSkybox(cubemap);

			ImGui::TreePop();
			UI::EndPropertyGrid();
		}

		bool bUpdatedOptions = false;

		ImGui::Separator();
		UI::BeginPropertyGrid("SceneGammaSettings");

		if (UI::PropertyDrag("Gamma", rendererOptions.Gamma, 0.1f, 0.0f, 10.f))
			bUpdatedOptions = true;

		bUpdatedOptions |= UI::PropertyDrag("Exposure", rendererOptions.Exposure, 0.1f, 0.0f, 100.f);
		bUpdatedOptions |= UI::ComboEnum<TonemappingMethod>("Tonemapping", rendererOptions.Tonemapping);

		UI::EndPropertyGrid();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		treeOpened = ImGui::TreeNodeEx((void*)treeID2, flags, "Photo Linear Tonemapping Settings");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("PhotoSettings");

			auto params = rendererOptions.PhotoLinearTonemappingParams;
			bool bChanged = false;
			bChanged |= UI::PropertyDrag("Sensetivity", params.Sensetivity, 0.01f);
			bChanged |= UI::PropertyDrag("Exposure time (s)", params.ExposureTime, 0.01f);
			bChanged |= UI::PropertyDrag("F-Stop", params.FStop, 0.01f);

			if (bChanged)
				rendererOptions.PhotoLinearTonemappingParams = params;

			bUpdatedOptions |= bChanged;

			ImGui::TreePop();
			UI::EndPropertyGrid();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		ImGui::Separator();
		treeOpened = ImGui::TreeNodeEx((void*)treeID3, flags, "Filmic Tonemapping Settings");
		ImGui::PopStyleVar();
		if (treeOpened)
		{
			UI::BeginPropertyGrid("FilmicSettings");

			bUpdatedOptions |= UI::PropertyDrag("White Point", rendererOptions.FilmicTonemappingParams.WhitePoint, 0.05f);

			ImGui::TreePop();
			UI::EndPropertyGrid();
		}

		if (bUpdatedOptions)
			sceneRenderer->SetOptions(rendererOptions);

		ImGui::End();
		ImGui::PopID();
	}

	void EditorLayer::DrawSettings()
	{
		ImGui::Begin("Settings");
		UI::BeginPropertyGrid("SettingsPanel");

		auto& sceneRenderer = m_CurrentScene->GetSceneRenderer();
		SceneRendererSettings options = sceneRenderer->GetOptions();
		bool bSettingsChanged = false;

		if (UI::Property("VSync", m_VSync))
		{
			EG_EDITOR_TRACE("Changed VSync to: {}", m_VSync);
			Application::Get().GetWindow().SetVSync(m_VSync);
		}

		bSettingsChanged |= UI::Property("Enable Soft Shadows", options.bEnableSoftShadows);
		bSettingsChanged |= UI::Property("Enable Shadows smooth transition", options.bEnableCSMSmoothTransition, "Enable smooth transition of cascaded shadows (affects shadows that are casted by directional light)");
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

		// Ambient Occlusion method
		{
			if (UI::ComboEnum<AmbientOcclusion>("Ambient Occlusion", options.AO))
			{
				bSettingsChanged = true;
				EG_EDITOR_TRACE("Changed AO to: {}", magic_enum::enum_name(options.AO));

				if (options.AO != AmbientOcclusion::SSAO)
				{
					if (m_VisualizingGBufferType == GBufferVisualizingType::SSAO)
					{
						SetVisualizingBufferType(GBufferVisualizingType::Final);
						m_SelectedBufferIndex = -1;
						m_ViewportImage = &sceneRenderer->GetOutput();
					}
				}
				else if (options.AO != AmbientOcclusion::GTAO)
				{
					if (m_VisualizingGBufferType == GBufferVisualizingType::GTAO)
					{
						SetVisualizingBufferType(GBufferVisualizingType::Final);
						m_SelectedBufferIndex = -1;
						m_ViewportImage = &sceneRenderer->GetOutput();
					}
				}
			}
		}
		UI::EndPropertyGrid();

		constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

		// Bloom settings
		{
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
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
					EG_EDITOR_TRACE("Changed Bloom Dirt Texture to: {}", settings.Dirt ? "None" : settings.Dirt->GetPath());
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}
		}
		
		// SSAO settings
		{
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
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

				if (UI::PropertyDrag("Samples", samples, 2))
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
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
			ImGui::Separator();
			bool treeOpened = ImGui::TreeNodeEx("GTAO Settings", treeFlags);
			ImGui::PopStyleVar();
			if (treeOpened)
			{
				UI::BeginPropertyGrid("GTAO Settings");

				GTAOSettings& settings = options.GTAOSettings;
				int samples = (int)settings.GetNumberOfSamples();
				float radius = settings.GetRadius();

				if (UI::PropertyDrag("Samples", samples, 2))
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

		// Fog settings
		{
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = (GImGui->Font->FontSize * GImGui->Font->Scale) + GImGui->Style.FramePadding.y * 2.f;
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

		ImGuiLayer::ShowStyleSelector("Style", m_EditorStyle);

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
				auto stats = RenderManager::GetStats();

				ImGui::Text("Draw Calls: %d", stats.DrawCalls);
				ImGui::Text("Vertices: %d", stats.Vertices);
				ImGui::Text("Indices: %d", stats.Indeces);

				ImGui::TreePop();
			}

			bool renderer2DTreeOpened = ImGui::TreeNodeEx((void*)"Renderer2D", flags, "Renderer2D Stats");
			if (renderer2DTreeOpened)
			{
				auto stats = RenderManager::GetStats2D();

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
		static SceneRendererSettings prevRendererSettings;

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
		{
			if (m_EditorState == EditorState::Edit)
			{
				m_EditorState = EditorState::Play;
				prevRendererSettings = m_EditorScene->GetSceneRenderer()->GetOptions();
				m_SimulationScene = MakeRef<Scene>(m_EditorScene);
				SetCurrentScene(m_SimulationScene);
				m_SimulationScene->OnRuntimeStart();
				m_PlaySound->Play();
				EG_EDITOR_TRACE("Editor Play pressed");
			}
			else if (m_EditorState != EditorState::Edit)
			{
				m_SimulationScene->OnRuntimeStop();
				m_EditorState = EditorState::Edit;
				m_SimulationScene.reset();
				SetCurrentScene(m_EditorScene);
				m_EditorScene->GetSceneRenderer()->SetOptions(prevRendererSettings);
				EG_EDITOR_TRACE("Editor Stop pressed");
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	void EditorLayer::Submit(const std::function<void()>& func)
	{
		std::scoped_lock lock(s_DeferredCallsMutex);
		m_DeferredCalls.push_back(func);
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
			ImGui::BulletText("To detach one entity from another, drag & drop it on top of 'Scene Hierarchy' panel name.");
			ImGui::BulletText("Supported texture format: 4 channel PNG, 3 channel JPG");
			ImGui::BulletText("Supported 3D-Model formats: fbx, blend, 3ds, obj, smd, vta, stl.\nNote that a single file can contain multiple meshes. If a model containes information about textures, Engine will try to load them as well.");
			ImGui::BulletText("Supported audio formats: wav, ogg, wma");
			ImGui::TreePop();
		}
		ImGui::Separator();

		if (ImGui::TreeNodeEx("Third party", flags, "Third party"))
		{
			// TODO: Add versions and links & icons for physx and fmod
			ImGui::SetWindowFontScale(1.2f);
			ImGui::BulletText("assimp");
			ImGui::BulletText("EnTT");
			ImGui::BulletText("glad");
			ImGui::BulletText("GLFW");
			ImGui::BulletText("glm");
			ImGui::BulletText("ImGui");
			ImGui::BulletText("ImGuizmo");
			ImGui::BulletText("Mono");
			ImGui::BulletText("PhysX");
			ImGui::BulletText("spdlog");
			ImGui::BulletText("stb_image");
			ImGui::BulletText("thread-pool");
			ImGui::BulletText("vma");
			ImGui::BulletText("yaml-cpp");
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
