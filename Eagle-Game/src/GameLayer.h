#pragma once

#include "Eagle.h"

namespace Eagle
{
	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Game Layer") {}

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

	private:
		bool OnKeyPressed(KeyPressedEvent& e);
		void LoadRendererSettings();

	private:
		SceneRendererSettings m_RendererSettings;

		Ref<Scene> m_DummyScene;
		Ref<Scene> m_CurrentScene;
		
		GUID m_OpenedSceneCallbackID;
		glm::uvec2 m_WindowSize = {};
		
		bool m_WindowFocused = true;
		bool m_DrawUI = false;
		bool m_VSync = true;
		bool m_Fullscreen = true;
	};
}
