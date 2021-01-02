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
	m_CameraController(1280.f / 720.f)
{
	using namespace Eagle;

	float squarePositions[] =
	{
		//Pos				//TexCoord
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
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

	m_GradientShader = Eagle::Shader::Create("Gradient", gradientVertexSource, gradientFragmentSource);
	m_ShaderLibrary.Load("BasicTexture", "assets/shaders/TextureShader.glsl");

	m_NaviTexture = Eagle::Texture2D::Create("assets/textures/test.png");
	m_NaviTexture->Bind();

	m_MainMenuTexture = Eagle::Texture2D::Create("assets/textures/mainmenu.png");
	m_MainMenuTexture->Bind();
}

void ExampleLayer::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;

	m_CameraController.OnUpdate(ts);

	Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Renderer::Clear();

	static glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));

	Renderer::BeginScene(m_CameraController.GetCamera());
	m_GradientShader->Bind();

	std::dynamic_pointer_cast<OpenGLShader>(m_GradientShader)->SetUniformFloat("u_Time", (float)glfwGetTime());

	auto textureShader = m_ShaderLibrary.Get("BasicTexture");

	textureShader->Bind();
	std::dynamic_pointer_cast<OpenGLShader>(textureShader)->SetUniformFloat("u_Texture", 0);

	for (int y = -10; y < 22; ++y)
	{
		for (int x = -10; x < 22; ++x)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(x * 0.16f, y * 0.09f, 0.f));
			Renderer::Submit(m_GradientShader, m_SquareVA, transform * scale);
		}
	}

	m_NaviTexture->Bind();
	Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.f), glm::vec3(1.0f)));

	m_MainMenuTexture->Bind();
	Renderer::Submit(textureShader, m_SquareVA, glm::scale(glm::mat4(1.f), glm::vec3(1.0f)));

	Renderer::EndScene();
}

void ExampleLayer::OnEvent(Eagle::Event& e)
{
	m_CameraController.OnEvent(e);
}

void ExampleLayer::OnImGuiRender()
{
}

Eagle::Application* Eagle::CreateApplication()
{
	return new Sandbox;
}