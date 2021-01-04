#include "egpch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"

#include "VertexArray.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace Eagle
{

	struct Renderer2DData
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> FlatColorShader;
	};

	static Scope<Renderer2DData> s_Data;
		
	void Renderer2D::Init()
	{
		s_Data = MakeScope<Renderer2DData>();

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

		s_Data->QuadVertexArray = VertexArray::Create();
		s_Data->QuadVertexArray->AddVertexBuffer(squareVertexBuffer);
		s_Data->QuadVertexArray->SetIndexBuffer(squareIndexBuffer);

		s_Data->FlatColorShader = Eagle::Shader::Create("assets/shaders/FlatColor.glsl");
	}

	void Renderer2D::Shutdown()
	{
		s_Data.reset(nullptr);
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		s_Data->FlatColorShader->Bind();
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->FlatColorShader)->SetUniformMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->FlatColorShader)->SetUniformMat4("u_Transform", glm::mat4(1.f));
	}

	void Renderer2D::EndScene()
	{
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		s_Data->FlatColorShader->Bind();
		std::dynamic_pointer_cast<OpenGLShader>(s_Data->FlatColorShader)->SetUniformFloat4("u_Color", color);

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({position.x, position.y, 0.0f}, size, color);
	}
}

