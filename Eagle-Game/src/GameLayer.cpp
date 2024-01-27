#include "GameLayer.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Core/Serializer.h"
#include "Eagle/Core/SceneSerializer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Eagle
{
	void GameLayer::OnAttach()
	{
		AssetManager::Init();
		auto& window = Application::Get().GetWindow();
		
		const auto& projectInfo = Project::GetProjectInfo();
		const auto& startScene = projectInfo.GameStartupScene;
		if (startScene)
		{
			const auto& scenePath = startScene->GetPath();
			Ref<Scene> scene = MakeRef<Scene>(scenePath.filename().u8string(), nullptr, true);
			YAML::Node sceneNode;
			if (AssetManager::GetRuntimeAssetNode(scenePath, &sceneNode))
			{
				SceneSerializer serializer(scene);
				serializer.Deserialize(sceneNode);
			}
			m_CurrentScene = std::move(scene);
		}
		else
		{
			m_DummyScene = MakeRef<Scene>("Dummy scene");
			m_CurrentScene = m_DummyScene;
			m_CurrentScene->bDrawMiscellaneous = false;
		}

		LoadRendererSettings();
		window.SetVSync(m_VSync);
		window.SetWindowTitle(projectInfo.Name);
		window.SetFullscreen(m_Fullscreen);
		m_WindowSize = { window.GetWidth(), window.GetHeight() };

		m_CurrentScene->GetSceneRenderer()->SetOptions(m_RendererSettings);
		m_CurrentScene->OnViewportResize((uint32_t)m_WindowSize.x, (uint32_t)m_WindowSize.y);
		Scene::SetCurrentScene(m_CurrentScene);
		if (m_CurrentScene != m_DummyScene)
		{
			ScriptEngine::LoadAppAssembly(Project::GetProjectInfo().Name + ".dll");
			m_CurrentScene->OnRuntimeStart();
		}

		Scene::AddOnSceneOpenedCallback(m_OpenedSceneCallbackID, [this](const Ref<Scene>& scene)
		{
			if (m_CurrentScene && (m_DummyScene != m_CurrentScene))
				m_CurrentScene->OnRuntimeStop();
			m_CurrentScene = scene;
			Scene::SetCurrentScene(m_CurrentScene);

			scene->OnViewportResize((uint32_t)m_WindowSize.x, (uint32_t)m_WindowSize.y);
			scene->OnRuntimeStart();

			EG_CORE_INFO("Opened a scene: {}", scene->GetDebugName());
		});
	}
	
	void GameLayer::OnDetach()
	{
		m_DummyScene.reset();
		m_CurrentScene.reset();
		Scene::SetCurrentScene(nullptr);
		Scene::RemoveOnSceneOpenedCallback(m_OpenedSceneCallbackID);
	}
	
	void GameLayer::OnUpdate(Timestep ts)
	{
		m_CurrentScene->OnUpdate(ts, m_WindowFocused);
	}
	
	void GameLayer::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::WindowFocused)
			m_WindowFocused = ((const WindowFocusedEvent&)e).IsFocused();
		else if (e.GetEventType() == EventType::WindowResize)
		{
			const WindowResizeEvent& resizeEvent = (const WindowResizeEvent&)e;
			glm::uvec2 windowSize = { resizeEvent.GetWidth(), resizeEvent.GetHeight() };
			if (windowSize.x != 0 && windowSize.y != 0)
			{
				m_WindowSize = windowSize;
				m_CurrentScene->OnViewportResize(m_WindowSize.x, m_WindowSize.y);
			}
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(EG_BIND_FN(GameLayer::OnKeyPressed));

		m_CurrentScene->OnEventRuntime(e);
	}
	
	void GameLayer::OnImGuiRender()
	{
		if (!m_DrawUI)
			return;
		
		EG_CPU_TIMING_SCOPED("GameLayer. UI");
		
		if (ImGui::Begin("Game Layer"))
		{
			if (ImGui::Button("Open the test scene"))
			{
				Ref<Asset> asset;
				if (AssetManager::Get("Content/Scenes/test.egasset", &asset))
					Scene::OpenScene(Cast<AssetScene>(asset), true, true);
			}
			if (ImGui::Button("Open the test copy scene"))
			{
				Ref<Asset> asset;
				if (AssetManager::Get("Content/Scenes/test_Copy.egasset", &asset))
					Scene::OpenScene(Cast<AssetScene>(asset), true, true);
			}
			ImGui::Text("FPS: %d", int(1.f / Application::Get().GetTimestep()));
			ImGui::Separator();
			if (ImGui::Button("Exit"))
				Application::Get().SetShouldClose(true);
		}
		ImGui::End();
	}
	
	bool GameLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		//Shortcuts
		if (e.GetRepeatCount() > 0)
			return false;

		const bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		const bool leftShift = Input::IsKeyPressed(Key::LeftShift);
		const bool rightShift = Input::IsKeyPressed(Key::RightShift);
		const bool shift = leftShift || rightShift;

		const Key pressedKey = e.GetKey();
		switch (pressedKey)
		{
		case Key::F1:
			if (control && shift)
				m_DrawUI = !m_DrawUI;
		}

		return false;
	}
	
	void GameLayer::LoadRendererSettings()
	{
		const char* configFilepath = "Config/RenderConfig.ini";
		if (!std::filesystem::exists(configFilepath))
			return;
		
		YAML::Node baseNode = YAML::LoadFile(configFilepath);
		if (auto node = baseNode["VSync"])
			m_VSync = node.as<bool>();
		if (auto node = baseNode["Fullscreen"])
			m_Fullscreen = node.as<bool>();
		Serializer::DeserializeRendererSettings(baseNode, m_RendererSettings);
	}
}
