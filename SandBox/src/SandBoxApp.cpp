#include "SandBoxApp.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/include/GLFW/glfw3.h>
#include "Eagle/Core.h"

Sandbox::Sandbox()
{
	PushLayer(new ExampleLayer());
	m_Window->SetVSync(true);
}

ExampleLayer::ExampleLayer()
	: Layer("ExampleLayer"),
	m_Camera(0.f, 1280.f, 0.f, 720.f),
	m_MouseX(0.f),
	m_MouseY(0.f)
{
	using namespace Eagle;

	float squarePositions[] =
	{
		//Pos								//TexCoord
		1280.f * 0.25f, 720.f * 0.25f, 0.0f, 0.0f, 0.0f,
		1280.f * 0.75f, 720.f * 0.25f, 0.0f, 1.0f, 0.0f,
		1280.f * 0.75f, 720.f * 0.75f, 0.0f, 1.0f, 1.0f,
		1280.f * 0.25f, 720.f * 0.75f, 0.0f, 0.0f, 1.0f
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
		{ShaderDataType::Float3, "a_Position"},
		{ShaderDataType::Float2, "a_TexCoord"}
	};
	squareVertexBuffer->SetLayout(squareLayout);

	m_SquareVA = VertexArray::Create();
	m_SquareVA->AddVertexBuffer(squareVertexBuffer);
	m_SquareVA->SetIndexBuffer(squareIndexBuffer);

	std::string gradientVertexSource = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Positions;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;

			void main()
			{
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Positions, 1.0);
			}
		)";

	std::string gradientFragmentSource = R"(
			#version 330 core

			out vec4 color;

			uniform float u_Time;

			void main()
			{
				vec2 resolution = vec2(1280.f, 720.f);
				float u_Cover = 1.f;

				vec2 uv = gl_FragCoord.xy / resolution.xy;
				
				vec3 col = 0.5f + 0.5f * cos(u_Time + uv.xyx + vec3(0, 2, 4));
				vec3 temp = col * sin(u_Cover - uv.y) * 0.5f;
				
				color = vec4(temp, 1.f) * 2.f;

				//color = vec4(0.2, 0.2, 0.8, 1.0);
			}
		)";

	m_GradientShader = Eagle::Shader::Create(gradientVertexSource, gradientFragmentSource);

	m_TextureShader = Eagle::Shader::Create("assets/shaders/TextureShader.glsl");

	m_NaviTexture = Eagle::Texture2D::Create("assets/textures/test.png");
	m_NaviTexture->Bind();

	m_MainMenuTexture = Eagle::Texture2D::Create("assets/textures/mainmenu.png");
	m_MainMenuTexture->Bind();
}

void ExampleLayer::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;

	Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Renderer::Clear();

	static glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));

	Renderer::BeginScene(m_Camera);
	m_GradientShader->Bind();

	std::dynamic_pointer_cast<OpenGLShader>(m_GradientShader)->SetUniformFloat("u_Time", (float)glfwGetTime());

	m_TextureShader->Bind();
	std::dynamic_pointer_cast<OpenGLShader>(m_TextureShader)->SetUniformFloat("u_Texture", 0);

	for (int y = 0; y < 32; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(x * 25 * 1.6f, y * 25 * 0.9f, 0.f));
			Renderer::Submit(m_GradientShader, m_SquareVA, transform * scale);
		}
	}

	m_NaviTexture->Bind();
	Renderer::Submit(m_TextureShader, m_SquareVA, glm::scale(glm::mat4(1.f), glm::vec3(1.25f)));

	m_MainMenuTexture->Bind();
	Renderer::Submit(m_TextureShader, m_SquareVA, glm::scale(glm::mat4(1.f), glm::vec3(1.25f)));

	Renderer::EndScene();

	if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
	{
		float offsetX = m_MouseX - Input::GetMouseX();
		float offsetY = Input::GetMouseY() - m_MouseY;

		glm::vec3 cameraNewPos = m_Camera.GetPosition();
		cameraNewPos.x += offsetX;
		cameraNewPos.y += offsetY;

		m_Camera.SetPosition(cameraNewPos);
	}
	
	m_MouseX = Input::GetMouseX();
	m_MouseY = Input::GetMouseY();
}

void ExampleLayer::OnEvent(Eagle::Event& e)
{
}

void ExampleLayer::OnImGuiRender()
{
}

Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}