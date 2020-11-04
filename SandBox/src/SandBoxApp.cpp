#include "Eagle.h"

class ExampleLayer : public Eagle::Layer
{
public:
	ExampleLayer() : Layer("ExampleLayer") {}

	void OnUpdate() override
	{
		//EG_INFO("ExampleLayer::Update");
	}

	void OnEvent(Eagle::Event& e) override
	{
		//EG_TRACE("{0}", e);
	}
};

class Sandbox : public Eagle::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
		PushLayout(new Eagle::ImGuiLayer());
	}
};

Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}

