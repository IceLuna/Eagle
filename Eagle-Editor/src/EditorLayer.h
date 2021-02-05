#pragma once

#include "Eagle.h"

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
		bool m_DepthAttachment = false;
	};
}
