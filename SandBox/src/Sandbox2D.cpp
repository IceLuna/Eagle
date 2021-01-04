#include "SandBox2D.h"

#include <Platform/OpenGL/OpenGLShader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Eagle/Core/Core.h>

#include <imgui/imgui.h>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"),
	m_CameraController(1280.f / 720.f)
{
	
}

void Sandbox2D::OnAttach()
{
}

void Sandbox2D::OnDetach()
{
}

void Sandbox2D::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;

	m_CameraController.OnUpdate(ts);

	Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Renderer::Clear();

	Renderer2D::BeginScene(m_CameraController.GetCamera());

	Renderer2D::DrawQuad({0.f, 0.f}, {1.f, 1.f}, m_SquareColor);

	Renderer::EndScene();
}

void Sandbox2D::OnEvent(Eagle::Event& e)
{
	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();
}
