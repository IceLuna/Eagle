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
	Eagle::Ref<Eagle::VertexArray> m_SquareVA;
	Eagle::Ref<Eagle::Shader> m_FlatColorShader;

	glm::vec4 m_SquareColor1 = {0.8f, 0.2f, 0.7f, 1.f};
	glm::vec4 m_SquareColor2 = {0.3f, 0.2f, 0.8f, 1.f};

	Eagle::OrthographicCameraController m_CameraController;
};
