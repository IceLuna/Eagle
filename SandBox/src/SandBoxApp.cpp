#include "Eagle.h"

class ExampleLayer : public Eagle::Layer
{
public:
	ExampleLayer() : Layer("ExampleLayer") {}

	void OnUpdate() override
	{
	}

	void OnEvent(Eagle::Event& e) override
	{
		//EG_TRACE("{0}", e);
	}

	void OnImGuiRender() override
	{
		
	}
};

class Sandbox : public Eagle::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}
};

Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}

