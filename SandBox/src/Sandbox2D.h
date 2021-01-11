#pragma once

#include "Eagle.h"

class Sandbox2D : public Eagle::Layer
{
public:
	Sandbox2D();

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(Eagle::Timestep ts) override;
	void OnEvent(Eagle::Event& e) override;
	void OnImGuiRender() override;

private:

	Eagle::TextureProps textureProps;
	Eagle::Ref<Eagle::Texture2D> m_Texture;
	Eagle::Ref<Eagle::Texture2D> m_SpriteSheet;

	Eagle::Ref<Eagle::SubTexture2D> m_StairTexture;
	Eagle::Ref<Eagle::SubTexture2D> m_BarrelTexture;
	Eagle::Ref<Eagle::SubTexture2D> m_TreeTexture;

	Eagle::Ref<Eagle::Framebuffer> m_Framebuffer;
	Eagle::FramebufferSpecification m_FramebufferSpec;

	Eagle::OrthographicCameraController m_CameraController;

	glm::vec4 m_SquareColor1 = { 0.8f, 0.2f, 0.7f, 1.f };
	glm::vec4 m_SquareColor2 = { 0.3f, 0.2f, 0.8f, 1.f };
	Eagle::Timestep m_Ts;
};
