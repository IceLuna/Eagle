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
	Eagle::Ref<Eagle::VertexArray> m_SquareVA;
	Eagle::Ref<Eagle::Shader> m_GradientShader;

	Eagle::ShaderLibrary m_ShaderLibrary;
	Eagle::Ref<Eagle::Texture> m_NaviTexture;
	Eagle::Ref<Eagle::Texture> m_MainMenuTexture;

	Eagle::OrthographicCamera m_Camera;
	float m_MouseX;
	float m_MouseY;
};

class Sandbox : public Eagle::Application
{
public:
	Sandbox();
};