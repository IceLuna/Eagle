#include "egpch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"

#include "VertexArray.h"
#include "Shader.h"

#include <glm/gtc/matrix_transform.hpp>


namespace Eagle
{

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		//TODO: texture
	};

	struct Renderer2DData
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> UniqueShader;
		Ref<Texture> WhiteTexture;

		QuadVertex* QuadVertexBase = nullptr;
		QuadVertex* QuadVertexPtr = nullptr;

		uint32_t IndicesCount = 0;

		const uint32_t MaxQuads = 10000;
		const uint32_t MaxVertices = MaxQuads * 4;
		const uint32_t MaxIndices = MaxQuads * 6;
	};

	static Renderer2DData s_Data;
		
	void Renderer2D::Init()
	{
		uint32_t* quadIndeces = new uint32_t[s_Data.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6u)
		{
			quadIndeces[i + 0] = offset + 0; 
			quadIndeces[i + 1] = offset + 1; 
			quadIndeces[i + 2] = offset + 2;
			
			quadIndeces[i + 3] = offset + 2; 
			quadIndeces[i + 4] = offset + 3; 
			quadIndeces[i + 5] = offset + 0; 

			offset += 4;
		}

		Ref<IndexBuffer> quadIndexBuffer;
		quadIndexBuffer = IndexBuffer::Create(quadIndeces, s_Data.MaxIndices);
		delete[] quadIndeces;

		BufferLayout squareLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float4, "a_Color"},
			{ShaderDataType::Float2, "a_TexCoord"}
		};
		
		s_Data.QuadVertexBuffer = VertexBuffer::Create(sizeof(QuadVertex) * s_Data.MaxQuads);
		s_Data.QuadVertexBuffer->SetLayout(squareLayout);

		s_Data.QuadVertexArray = VertexArray::Create();
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIndexBuffer);

		s_Data.QuadVertexBase = new QuadVertex[s_Data.MaxVertices];

		s_Data.UniqueShader = Shader::Create("assets/shaders/UniqueShader.glsl");
		
		uint32_t whitePixel = 0xffffffff;
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		s_Data.WhiteTexture->SetData(&whitePixel);
	}

	void Renderer2D::Shutdown()
	{
	}

	void Renderer2D::BeginScene(const OrthographicCamera& camera)
	{
		const glm::mat4& cameraVP = camera.GetViewProjectionMatrix();
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetMat4("u_ViewProjection", cameraVP);
		//s_Data.UniqueShader->SetInt("u_Texture", 0);

		s_Data.IndicesCount = 0;
		s_Data.QuadVertexPtr = s_Data.QuadVertexBase;
	}

	void Renderer2D::EndScene()
	{
		uint32_t dataSize = (uint8_t*)s_Data.QuadVertexPtr - (uint8_t*)s_Data.QuadVertexBase;

		s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBase, dataSize);
		Flush();
	}

	void Renderer2D::Flush()
	{
		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.IndicesCount);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		s_Data.QuadVertexPtr->Position = position;
		s_Data.QuadVertexPtr->Color = color;
		s_Data.QuadVertexPtr->TexCoord = { 0.f, 0.f };
		++s_Data.QuadVertexPtr;

		s_Data.QuadVertexPtr->Position = { position.x + size.x, position.y, 0.f};
		s_Data.QuadVertexPtr->Color = color;
		s_Data.QuadVertexPtr->TexCoord = { 1.f, 0.f };
		++s_Data.QuadVertexPtr;

		s_Data.QuadVertexPtr->Position = { position.x + size.x, position.y + size.y, 0.f };
		s_Data.QuadVertexPtr->Color = color;
		s_Data.QuadVertexPtr->TexCoord = { 1.f, 1.f };
		++s_Data.QuadVertexPtr;

		s_Data.QuadVertexPtr->Position = { position.x, position.y + size.y, 0.f };
		s_Data.QuadVertexPtr->Color = color;
		s_Data.QuadVertexPtr->TexCoord = { 0.f, 1.f };
		++s_Data.QuadVertexPtr;

		s_Data.IndicesCount += 6;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({position.x, position.y, 0.0f}, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data.UniqueShader->SetMat4("u_Transform", transform);
		s_Data.UniqueShader->SetFloat4("u_Color", glm::vec4(textureProps.TintFactor.r, textureProps.TintFactor.g, textureProps.TintFactor.b, textureProps.Opacity));
		s_Data.UniqueShader->SetFloat("u_TilingFactor", textureProps.TilingFactor);
		texture->Bind();

		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.QuadVertexArray);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, textureProps);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::rotate(transform, radians, glm::vec3(0, 0, 1));
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data.UniqueShader->SetMat4("u_Transform", transform);
		s_Data.UniqueShader->SetFloat4("u_Color", color);
		s_Data.UniqueShader->SetFloat("u_TilingFactor", 1.f);
		s_Data.WhiteTexture->Bind();

		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.QuadVertexArray);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const glm::vec4& color)
	{
		DrawRotatedQuad({ position.x, position.y, 0.0f }, size, radians, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
		transform = glm::rotate(transform, radians, glm::vec3(0, 0, 1));
		transform = glm::scale(transform, { size.x, size.y, 1.f });

		s_Data.UniqueShader->SetMat4("u_Transform", transform);
		s_Data.UniqueShader->SetFloat4("u_Color", glm::vec4(textureProps.TintFactor.r, textureProps.TintFactor.g, textureProps.TintFactor.b, textureProps.Opacity));
		s_Data.UniqueShader->SetFloat("u_TilingFactor", textureProps.TilingFactor);
		texture->Bind();

		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.QuadVertexArray);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size, float radians, const Ref<Texture2D>& texture, const TextureProps& textureProps)
	{
		DrawRotatedQuad({position.x, position.y, 0.f}, size, radians, texture, textureProps);
	}
}

