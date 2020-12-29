#pragma once

#include "Eagle.h"


class ExampleLayer : public Eagle::Layer
{
public:
	ExampleLayer();

	void OnUpdate(Eagle::Timestep ts) override;

	void OnEvent(Eagle::Event& e) override;

	void OnImGuiRender() override;

private:
	std::shared_ptr<Eagle::VertexArray> m_VertexArray;
	std::shared_ptr<Eagle::Shader> m_Shader;

	std::shared_ptr<Eagle::VertexArray> m_SquareVA;
	std::shared_ptr<Eagle::Shader> m_BlueShader;

	Eagle::OrthographicCamera m_Camera;
	float m_MouseX;
	float m_MouseY;
};

class Sandbox : public Eagle::Application
{
public:
	Sandbox();
};