#pragma once

#include "Eagle.h"
#include "Panels/SceneHierarchyPanel.h"

namespace Eagle
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;
		void OnImGuiRender() override;

	private:
		bool OnKeyPressed(KeyPressedEvent& e);

		void NewScene();
		void OpenScene();
		void SaveSceneAs();

	private:
		EditorCamera m_EditorCamera;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		TextureProps textureProps;
		Ref<Texture2D> m_Texture;
		Ref<Texture2D> m_SpriteSheet;

		Entity m_SquareEntity;
		Ref<Scene> m_ActiveScene;

		Ref<SubTexture2D> m_StairTexture;
		Ref<SubTexture2D> m_BarrelTexture;
		Ref<SubTexture2D> m_TreeTexture;

		Ref<Framebuffer> m_Framebuffer;

		glm::vec2 m_CurrentViewportSize = {1.f, 1.f};
		glm::vec2 m_NewViewportSize = {1.f, 1.f};
		Timestep m_Ts;

		int m_GuizmoType = -1;
		
		bool m_DepthAttachment = false;
		bool m_ViewportHovered = true;
	};
}
