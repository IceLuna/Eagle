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
	Eagle::Ref<Eagle::VertexArray> m_VertexArray;
	Eagle::Ref<Eagle::Shader> m_Shader;

	Eagle::Ref<Eagle::VertexArray> m_SquareVA;
	Eagle::Ref<Eagle::Shader> m_BlueShader;

	Eagle::OrthographicCamera m_Camera;
	float m_MouseX;
	float m_MouseY;
};

class Sandbox : public Eagle::Application
{
public:
	Sandbox();
};