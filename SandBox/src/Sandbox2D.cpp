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
	using namespace Eagle;

	float squarePositions[] =
	{
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f
	};

	uint32_t squareIndeces[] =
	{
		0, 1, 2, 2, 3, 0
	};

	Eagle::Ref<VertexBuffer> squareVertexBuffer;
	Eagle::Ref<IndexBuffer> squareIndexBuffer;

	squareVertexBuffer = VertexBuffer::Create(squarePositions, sizeof(squarePositions));
	squareIndexBuffer = IndexBuffer::Create(squareIndeces, sizeof(squareIndeces) / sizeof(uint32_t));

	BufferLayout squareLayout =
	{
		{ShaderDataType::Float3, "a_Position"}
	};
	squareVertexBuffer->SetLayout(squareLayout);

	m_SquareVA = VertexArray::Create();
	m_SquareVA->AddVertexBuffer(squareVertexBuffer);
	m_SquareVA->SetIndexBuffer(squareIndexBuffer);

	m_FlatColorShader = Eagle::Shader::Create("assets/shaders/FlatColor.glsl");
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

	static glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));

	Renderer::BeginScene(m_CameraController.GetCamera());
	m_FlatColorShader->Bind();

	std::dynamic_pointer_cast<OpenGLShader>(m_FlatColorShader)->SetUniformFloat4("u_Color", m_SquareColor);

	Renderer::Submit(m_FlatColorShader, m_SquareVA, glm::scale(glm::mat4(1.f), glm::vec3(1.0f)));

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
