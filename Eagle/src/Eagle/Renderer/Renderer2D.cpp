#include "egpch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"

#include "VertexArray.h"
#include "Shader.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/EditorCamera.h"

#include "Eagle/Math/Math.h"

namespace Eagle
{

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		int EntityID = -1;
		int TextureSlotIndex = -1;
		float TilingFactor;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32;
		static const uint32_t StartTextureIndex = 1; //0 - white texture by default

		std::array<Ref<Texture>, MaxTextureSlots> TextureSlots;

		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<Shader> UniqueShader;
		Ref<Texture> WhiteTexture;

		QuadVertex* QuadVertexBase = nullptr;
		QuadVertex* QuadVertexPtr = nullptr;

		uint32_t IndicesCount = 0;
		uint32_t TextureIndex = StartTextureIndex;

		glm::vec4 QuadVertexPosition[4];

		Renderer2D::Statistics Stats;
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
			{ShaderDataType::Float2, "a_TexCoord"},
			{ShaderDataType::Int,	 "a_EntityID"},
			{ShaderDataType::Int,	 "a_TextureIndex"},
			{ShaderDataType::Float,	 "a_TilingFactor"}
		};
		
		s_Data.QuadVertexBuffer = VertexBuffer::Create(sizeof(QuadVertex) * s_Data.MaxVertices);
		s_Data.QuadVertexBuffer->SetLayout(squareLayout);

		s_Data.QuadVertexArray = VertexArray::Create();
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIndexBuffer);

		s_Data.QuadVertexBase = new QuadVertex[s_Data.MaxVertices];

		uint32_t whitePixel = 0xffffffff;
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		s_Data.WhiteTexture->SetData(&whitePixel);
		s_Data.TextureSlots[0] = s_Data.WhiteTexture;

		int32_t samplers[s_Data.MaxTextureSlots];

		for (int i = 0; i < s_Data.MaxTextureSlots; ++i)
		{
			samplers[i] = i;
		}

		s_Data.UniqueShader = Shader::Create("assets/shaders/UniqueShader.glsl");
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetIntArray("u_Textures", samplers, 32);

		s_Data.QuadVertexPosition[0] = {-0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[1] = { 0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[2] = { 0.5f,  0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[3] = {-0.5f,  0.5f, 0.f, 1.f};
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBase;
	}

	void Renderer2D::BeginScene(const CameraComponent& cameraComponent)
	{
		const glm::mat4 cameraVP = cameraComponent.GetViewProjection();
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetMat4("u_ViewProjection", cameraVP);

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& editorCamera)
	{
		const glm::mat4 cameraVP = editorCamera.GetViewProjection();
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetMat4("u_ViewProjection", cameraVP);

		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		Flush();
	}

	void Renderer2D::Flush()
	{
		if (s_Data.IndicesCount == 0)
			return;

		uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexPtr - (uint8_t*)s_Data.QuadVertexBase);
		s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBase, dataSize);

		for (uint32_t i = 0; i < s_Data.TextureIndex; ++i)
		{
			s_Data.TextureSlots[i]->Bind(i);
		}

		s_Data.QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.IndicesCount);
		
		++s_Data.Stats.DrawCalls;
	}

	void Renderer2D::NextBatch()
	{
		Flush();
		StartBatch();
	}

	void Renderer2D::StartBatch()
	{
		s_Data.IndicesCount = 0;
		s_Data.QuadVertexPtr = s_Data.QuadVertexBase;

		s_Data.TextureIndex = s_Data.StartTextureIndex;
	}

	void Renderer2D::DrawQuad(const Transform& transform, const glm::vec4& color, Entity entity)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, color, entity);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, Entity entity)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, texture, textureProps, entity);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, Entity entity)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, subtexture, textureProps, entity);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, Entity entity)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };
		constexpr int textureIndex = 0;
		constexpr float tilingFactor = 0.f;

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Color = color;
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = (int)entity.GetID();
			s_Data.QuadVertexPtr->TextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->TilingFactor = tilingFactor;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, Entity entity)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };

		glm::vec4 defaultColor = glm::vec4(1.f);
		defaultColor.a = textureProps.Opacity;

		uint32_t textureIndex = 0;

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.TextureIndex; ++i)
		{
			if ((*s_Data.TextureSlots[i]) == (*texture))
			{
				textureIndex = i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (s_Data.TextureIndex >= Renderer2DData::MaxTextureSlots)
				NextBatch();

			textureIndex = s_Data.TextureIndex;
			s_Data.TextureSlots[textureIndex] = texture;
			++s_Data.TextureIndex;
		}

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Color = defaultColor;
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = (int)entity.GetID();
			s_Data.QuadVertexPtr->TextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->TilingFactor = textureProps.TilingFactor;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, Entity entity)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		const glm::vec2* texCoords = subtexture->GetTexCoords();
		const Ref<Texture2D> texture = subtexture->GetTexture();

		glm::vec4 defaultColor = glm::vec4(1.f);
		defaultColor.a = textureProps.Opacity;

		uint32_t textureIndex = 0;

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.TextureIndex; ++i)
		{
			if ((*s_Data.TextureSlots[i]) == (*texture))
			{
				textureIndex = i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (s_Data.TextureIndex >= Renderer2DData::MaxTextureSlots)
			{
				NextBatch();
			}

			textureIndex = s_Data.TextureIndex;
			s_Data.TextureSlots[textureIndex] = texture;
			++s_Data.TextureIndex;
		}

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Color = defaultColor;
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = (int)entity.GetID();
			s_Data.QuadVertexPtr->TextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->TilingFactor = textureProps.TilingFactor;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Renderer2D::Statistics));
	}
	
	Renderer2D::Statistics Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}
}

