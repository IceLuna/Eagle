#include "SandBox2D.h"

#include <Eagle/Core/Core.h>
#include <imgui/imgui.h>
#include <glm/glm.hpp>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"),
	m_CameraController(1280.f / 720.f)
{
	
}

void Sandbox2D::OnAttach()
{
	m_Texture = Eagle::Texture2D::Create("assets/textures/test.png");
	m_SpriteSheet = Eagle::Texture2D::Create("assets/textures/RPGpack_sheet_2X.png");

	m_StairTexture = Eagle::SubTexture2D::CreateFromCoords(m_SpriteSheet, {7, 6}, {128, 128});
	m_BarrelTexture = Eagle::SubTexture2D::CreateFromCoords(m_SpriteSheet, {8, 2}, {128, 128});
	m_TreeTexture = Eagle::SubTexture2D::CreateFromCoords(m_SpriteSheet, {2, 1}, {128, 128}, {1, 2});
}

void Sandbox2D::OnDetach()
{

}

void Sandbox2D::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;
	EG_PROFILE_FUNCTION();

	m_Ts = ts;

	m_CameraController.OnUpdate(ts);

	{
		EG_PROFILE_SCOPE("Sandbox2D::Clear");
		Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		Renderer::Clear();
	}

	Renderer2D::ResetStats();

	{
		EG_PROFILE_SCOPE("Sandbox2D::Draw");
		static float rotation = 0.f;
		rotation += 45.f * ts;
		Renderer2D::BeginScene(m_CameraController.GetCamera());

		Renderer2D::DrawRotatedQuad({-1.0f,  -0.7f}, {0.7f, 0.7f}, glm::radians(rotation), m_SquareColor1);
		Renderer2D::DrawQuad({-0.5f,  0.20f, 0.9f}, {0.7f, 0.7f}, m_SquareColor1);
		Renderer2D::DrawQuad({ 0.5f, -0.25f}, {0.3f, 0.8f}, m_SquareColor2);
		Renderer2D::DrawQuad({ 0.2f,  0.2f, 0.2f}, {0.8f, 0.8f}, m_Texture, textureProps);

		Renderer2D::DrawRotatedQuad({ -2.f,  1.5f, 0.1f}, {1.f, 1.f}, glm::radians(rotation), m_StairTexture, textureProps);
		Renderer2D::DrawQuad({ -2.f,  0.f, 0.1f}, {1.f, 1.f}, m_BarrelTexture, textureProps);
		Renderer2D::DrawQuad({ -2.f,  -1.5f, 0.1f}, {1.f, 2.f}, m_TreeTexture, textureProps);

		Renderer2D::DrawRotatedQuad({ 0.2f,  -0.8f, 0.1f }, {0.8f, 0.8f}, glm::radians(rotation), m_Texture, textureProps);

		Renderer2D::EndScene();
	}
}

void Sandbox2D::OnEvent(Eagle::Event& e)
{
	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{
	EG_PROFILE_FUNCTION();

	auto stats = Eagle::Renderer2D::GetStats();
	ImGui::Begin("Stats");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetVertexCount());
	ImGui::Text("Indices: %d", stats.GetIndexCount());
	ImGui::Text("Frame Time: %.3fms", (float)m_Ts);
	ImGui::End();

	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color 1", &m_SquareColor1.r);
	ImGui::ColorEdit4("Square Color 2", &m_SquareColor2.r);
	ImGui::SliderFloat("Texture Opacity", &textureProps.Opacity, 0.0f, 1.0f);
	ImGui::SliderFloat("Texture Tiling", &textureProps.TilingFactor, 0.0f, 5.0f);
	ImGui::End();
}
