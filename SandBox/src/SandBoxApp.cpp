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

	float positions[] =
	{
		//Position			//Color
		1280.f * 0.25f, 720.f * 0.25f, 0.0f, 0.8f, 0.2f, 0.8f, 1.f,
		1280.f * 0.75f, 720.f * 0.25f, 0.0f, 0.8f, 0.8f, 0.2f, 1.f,
		1280.f * 0.50,  720.f * 0.75f, 0.0f, 0.1f, 0.8f, 0.1f, 1.f
	};

	uint32_t indeces[] =
	{
		0, 1, 2
	};

	Eagle::Ref<VertexBuffer> vertexBuffer;
	Eagle::Ref<IndexBuffer> indexBuffer;

	vertexBuffer = VertexBuffer::Create(positions, sizeof(positions));
	indexBuffer = IndexBuffer::Create(indeces, sizeof(indeces) / sizeof(uint32_t));

	BufferLayout layout =
	{
		{ShaderDataType::Float3, "a_Position"},
		{ShaderDataType::Float4, "a_Color"}
	};
	vertexBuffer->SetLayout(layout);

	m_VertexArray = VertexArray::Create();
	m_VertexArray->AddVertexBuffer(vertexBuffer);
	m_VertexArray->SetIndexBuffer(indexBuffer);

	float squarePositions[] =
	{
		1280.f * 0.25f, 720.f * 0.25f, 0.0f,
		1280.f * 0.75f, 720.f * 0.25f, 0.0f,
		1280.f * 0.75f, 720.f * 0.75f, 0.0f,
		1280.f * 0.25f, 720.f * 0.75f, 0.0f
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

	std::string VertexSource = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Positions;
			layout(location = 1) in vec4 a_Color;

			out vec3 v_Position;
			out vec4 v_Color;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;

			void main()
			{
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Positions, 1.0);
				v_Position = a_Positions;
				v_Color = a_Color;
			}
		)";

	std::string FragmentSource = R"(
			#version 330 core

			out vec4 color;

			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
		)";

	m_Shader = Eagle::Shader::Create(VertexSource, FragmentSource);

	std::string blueVertexSource = R"(
			#version 330 core

			layout(location = 0) in vec3 a_Positions;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;

			void main()
			{
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Positions, 1.0);
			}
		)";

	std::string blueFragmentSource = R"(
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

	m_BlueShader = Eagle::Shader::Create(blueVertexSource, blueFragmentSource);
}

void ExampleLayer::OnUpdate(Eagle::Timestep ts)
{
	using namespace Eagle;

	Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Renderer::Clear();

	static glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.05f));

	Renderer::BeginScene(m_Camera);
	m_BlueShader->Bind();

	std::dynamic_pointer_cast<OpenGLShader>(m_BlueShader)->SetUniformFloat("u_Time", (float)glfwGetTime());

	for (int y = 0; y < 32; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(x * 25 * 1.6f, y * 25 * 0.9f, 0.f));
			Renderer::Submit(m_BlueShader, m_SquareVA, transform * scale);
		}
	}

	Renderer::Submit(m_Shader, m_VertexArray);

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