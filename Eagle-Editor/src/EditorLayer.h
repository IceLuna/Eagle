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

		Ref<SubTexture2D> m_StairTexture;
		Ref<SubTexture2D> m_BarrelTexture;
		Ref<SubTexture2D> m_TreeTexture;

		Ref<Framebuffer> m_Framebuffer;
		FramebufferSpecification m_FramebufferSpec;

		OrthographicCameraController m_CameraController;

		glm::vec4 m_SquareColor1 = { 0.8f, 0.2f, 0.7f, 1.f };
		glm::vec4 m_SquareColor2 = { 0.3f, 0.2f, 0.8f, 1.f };
		Timestep m_Ts;
	};
}
