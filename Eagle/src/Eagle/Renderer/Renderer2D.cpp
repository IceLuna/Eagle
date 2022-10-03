#include "egpch.h"
#include "Renderer2D.h"
#include "Renderer.h"

#include "Buffer.h"
#include "Shader.h"
#include "Material.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/EditorCamera.h"

#include "Eagle/Math/Math.h"

namespace Eagle
{
	struct RendererMaterial
	{
	public:
		RendererMaterial() = default;
		RendererMaterial(const RendererMaterial&) = default;

		RendererMaterial(const Ref<Material>& material)
		: TintColor(material->TintColor), TilingFactor(material->TilingFactor), Shininess(material->Shininess)
		{}

		RendererMaterial& operator=(const RendererMaterial&) = default;
		RendererMaterial& operator=(const Ref<Material>& material)
		{
			TintColor = material->TintColor;
			TilingFactor = material->TilingFactor;
			Shininess = material->Shininess;

			return *this;
		}
	
	public:
		glm::vec4 TintColor = glm::vec4{ 1.f };
		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};

	struct QuadVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec3 Normal = glm::vec3{ 0.f };
		glm::vec3 ModelNormal = glm::vec3{ 0.f };
		glm::vec3 Tangent = glm::vec3{ 0.f };
		glm::vec2 TexCoord = glm::vec2{0.f};
		int EntityID = -1;
		int DiffuseTextureSlotIndex = -1;
		int SpecularTextureSlotIndex = -1;
		int NormalTextureSlotIndex = -1;
		RendererMaterial Material;
	};

	struct LineVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec4 Color = glm::vec4{0.f, 0.f, 0.f, 1.f};
	};

	struct Renderer2DData
	{
		static const uint32_t MaxLines = 5000;
		static const uint32_t MaxLineVertices = MaxLines * 2;
		static const uint32_t MaxQuads = 2000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;

		std::unordered_map<Ref<Texture>, int> BoundTextures;

		static constexpr glm::vec4 QuadVertexPosition[4] = { { -0.5f, -0.5f, 0.f, 1.f }, { 0.5f, -0.5f, 0.f, 1.f }, { 0.5f,  0.5f, 0.f, 1.f }, { -0.5f,  0.5f, 0.f, 1.f } };
		static constexpr glm::vec4 QuadVertexNormal[4] = { { 0.0f,  0.0f, -1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f, 0.0f },  { 0.0f,  0.0f, -1.0f, 0.0f } };

		Renderer2D::Statistics Stats;
		uint32_t FlushCounter = 0;
	};

	static Renderer2DData s_Data;
		
	void Renderer2D::Init()
	{
		
	}

	void Renderer2D::Shutdown()
	{
	}

	void Renderer2D::BeginScene()
	{
		
	}

	void Renderer2D::EndScene()
	{
		
	}

	void Renderer2D::Flush()
	{
		
	}

	void Renderer2D::NextBatch()
	{
		
	}

	void Renderer2D::StartBatch()
	{
		
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		DrawQuad(transformMatrix, material, entityID);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		DrawQuad(transformMatrix, subtexture, textureProps, entityID);
	}

	void Renderer2D::DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
	{
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID)
	{
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
	}

	void Renderer2D::DrawCurrentSkybox()
	{
	}

	void Renderer2D::DrawLines()
	{
	}

	void Renderer2D::ResetLinesData()
	{
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data.Stats, 0, sizeof(Renderer2D::Statistics));
	}
	
	Renderer2D::Statistics& Renderer2D::GetStats()
	{
		return s_Data.Stats;
	}
}

