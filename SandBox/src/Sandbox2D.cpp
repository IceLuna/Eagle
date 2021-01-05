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
	m_Texture = Eagle::Texture2D::Create("assets/textures/test.png");
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

	Renderer2D::DrawQuad({-0.5f,  0.20f}, {0.7f, 0.7f}, m_SquareColor1);
	Renderer2D::DrawQuad({ 0.5f, -0.25f}, {0.3f, 0.8f}, m_SquareColor2);
	Renderer2D::DrawQuad({ 0.2f,  0.2f, 0.1f}, {0.8f, 0.8f}, m_Texture, m_TextureOpacity);

	Renderer::EndScene();
}

void Sandbox2D::OnEvent(Eagle::Event& e)
{
	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color 1", glm::value_ptr(m_SquareColor1));
	ImGui::ColorEdit4("Square Color 2", glm::value_ptr(m_SquareColor2));
	ImGui::SliderFloat("Texture Opacity", &m_TextureOpacity, 0.0f, 1.0f);
	ImGui::End();
}
