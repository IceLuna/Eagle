#include "egpch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"

#include "VertexArray.h"

#include <glm/gtc/matrix_transform.hpp>


namespace Eagle
{

	struct Renderer2DData
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> UniqueShader;
		Ref<Texture> WhiteTexture;
		glm::mat4 VP;
	};

	static Scope<Renderer2DData> s_Data;
		
	void Renderer2D::Init()
	{
		s_Data = MakeScope<Renderer2DData>();

		float squarePositions[] =
		{
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0
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

		s_Data->QuadVertexArray = VertexArray::Create();
		s_Data->QuadVertexArray->AddVertexBuffer(squareVertexBuffer);
		s_Data->QuadVertexArray->SetIndexBuffer(squareIndexBuffer);

		s_Data->UniqueShader = Shader::Create("assets/shaders/UniqueShader.glsl");
		
		uint32_t blackPixel = 0x0;
		s_Data->WhiteTexture = Texture2D::Create(1, 1);
		s_Data->WhiteTexture->SetData(&blackPixel);
	}

	void Renderer2D::Shutdown()
	{
		s_Data.reset(nullptr);
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		s_Data->VP = camera.GetViewProjectionMatrix();
		s_Data->UniqueShader->Bind();
		s_Data->UniqueShader->SetMat4("u_ViewProjection", s_Data->VP);
		s_Data->UniqueShader->SetInt("u_Texture", 0);
	}

	void Renderer2D::EndScene()
	{
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		s_Data->UniqueShader->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::scale(transform, {size.x, size.y, 1.f});

		s_Data->UniqueShader->SetMat4("u_Transform", transform);
		s_Data->UniqueShader->SetFloat4("u_Color", color);
		s_Data->WhiteTexture->Bind();

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({position.x, position.y, 0.0f}, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		s_Data->UniqueShader->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data->UniqueShader->SetMat4("u_Transform", transform);
		s_Data->UniqueShader->SetFloat4("u_Color", glm::vec4(textureProps.TintFactor.r, textureProps.TintFactor.g, textureProps.TintFactor.b, textureProps.TintFactor.a));//Opacity
		s_Data->UniqueShader->SetFloat("u_TilingFactor", textureProps.TilingFactor);
		texture->Bind();

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, textureProps);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Shader>& shader)
	{
		shader->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		shader->SetMat4("u_Transform", transform);
		shader->SetMat4("u_ViewProjection", s_Data->VP);

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Shader>& shader)
	{
		DrawQuad({position.x, position.y, 0.f}, size, shader);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const glm::vec4& color)
	{
		s_Data->UniqueShader->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::rotate(transform, radians, glm::vec3(0, 0, 1));
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data->UniqueShader->SetMat4("u_Transform", transform);
		s_Data->UniqueShader->SetFloat4("u_Color", color);
		s_Data->UniqueShader->SetFloat("u_TilingFactor", 1.f);
		s_Data->WhiteTexture->Bind();

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, radians, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		s_Data->UniqueShader->Bind();

		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::rotate(transform, radians, glm::vec3(0, 0, 1));
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data->UniqueShader->SetMat4("u_Transform", transform);
		s_Data->UniqueShader->SetFloat4("u_Color", glm::vec4(textureProps.TintFactor.r, textureProps.TintFactor.g, textureProps.TintFactor.b, textureProps.TintFactor.a));//Opacity
		s_Data->UniqueShader->SetFloat("u_TilingFactor", textureProps.TilingFactor);
		texture->Bind();

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		DrawRotatedQuad({position.x, position.y, 0.f}, size, radians, texture, textureProps);
	}
}

