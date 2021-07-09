#include "egpch.h"
#include "Renderer2D.h"
#include "RenderCommand.h"

#include "VertexArray.h"
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
		: TilingFactor(material->TilingFactor), Shininess(material->Shininess)
		{}

		RendererMaterial& operator=(const RendererMaterial&) = default;
		RendererMaterial& operator=(const Ref<Material>& material)
		{
			TilingFactor = material->TilingFactor;
			Shininess = material->Shininess;

			return *this;
		}
	
	public:
		float TilingFactor = 1.f;
		float Shininess = 32.f;
	};

	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
		int EntityID = -1;
		int DiffuseTextureSlotIndex = -1;
		int SpecularTextureSlotIndex = -1;
		int NormalTextureSlotIndex = -1;
		RendererMaterial Material;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t SkyboxTextureIndex = 0;
		static const uint32_t BlackTextureIndex = 6;
		static const uint32_t StartTextureIndex = 7; //6 - black texture, 2-5 - point shadow map, 1 - dir shadow map, 0 - skybox
		static const uint32_t MaxTexturesIndex = 31;
		static const uint32_t DirectionalShadowTextureIndex = 1;
		static const uint32_t PointShadowTextureIndex = 2; //3, 4, 5

		std::unordered_map<Ref<Texture>, uint32_t> BoundTextures;

		Ref<Cubemap> CurrentSkybox;
		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<VertexArray> SkyboxVertexArray;
		Ref<VertexBuffer> SkyboxVertexBuffer;
		Ref<Shader> SpriteShader;
		Ref<Shader> NormalsShader;
		Ref<Shader> SkyboxShader;
		Ref<Shader> DirectionalShadowMapShader;
		Ref<Shader> PointShadowMapShader;
		Ref<Shader> CurrentShader;

		QuadVertex* QuadVertexBase = nullptr;
		QuadVertex* QuadVertexPtr = nullptr;

		uint32_t IndicesCount = 0;
		uint32_t SkyboxIndicesCount = 0;
		uint32_t CurrentTextureIndex = StartTextureIndex;

		glm::vec4 QuadVertexPosition[4];
		glm::vec4 QuadVertexNormal[4];

		Renderer2D::Statistics Stats;
		uint32_t FlushCounter = 0;
		bool bCanRedraw = false;
		bool bRedrawing = false;
	};

	static Renderer2DData s_Data;
		
	void Renderer2D::Init()
	{
		//Init Skybox Data
		float skyboxVertices[] = {
			// positions          
			-1.0f,  1.0f, -1.0f, //0
			-1.0f, -1.0f, -1.0f, //1
			 1.0f, -1.0f, -1.0f, //2
			 1.0f,  1.0f, -1.0f, //3
			-1.0f, -1.0f,  1.0f, //4
			-1.0f,  1.0f,  1.0f, //5
			 1.0f, -1.0f,  1.0f, //6
			 1.0f,  1.0f,  1.0f, //7
								 
		};
		uint32_t skyboxIndeces[] = 
		{
			0, 1, 2,
			2, 3, 0,
			4, 1, 0,
			0, 5, 4,
			2, 6, 7,
			7, 3, 2,
			4, 5, 7,
			7, 6, 4,
			7, 0, 3, //Top
			7, 5, 0, //Top
			6, 2, 1, //Bottom
			6, 1, 4	 //Bottom
		};

		s_Data.SkyboxIndicesCount = sizeof(skyboxIndeces) / sizeof(uint32_t);
		Ref<IndexBuffer> skyboxIndexBuffer;
		skyboxIndexBuffer = IndexBuffer::Create(skyboxIndeces, s_Data.SkyboxIndicesCount);

		BufferLayout skyboxLayout =
		{
			{ShaderDataType::Float3, "a_Position"}
		};

		s_Data.SkyboxVertexBuffer = VertexBuffer::Create(skyboxVertices, sizeof(skyboxVertices));
		s_Data.SkyboxVertexBuffer->SetLayout(skyboxLayout);

		s_Data.SkyboxVertexArray = VertexArray::Create();
		s_Data.SkyboxVertexArray->AddVertexBuffer(s_Data.SkyboxVertexBuffer);
		s_Data.SkyboxVertexArray->SetIndexBuffer(skyboxIndexBuffer);

		s_Data.SkyboxShader = ShaderLibrary::GetOrLoad("assets/shaders/SkyboxShader.glsl");
		s_Data.SkyboxVertexArray->Unbind();

		//Init Quad Data
		uint32_t* quadIndeces = new uint32_t[s_Data.MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data.MaxIndices; i += 6u)
		{
			quadIndeces[i + 0] = offset + 2; //0
			quadIndeces[i + 1] = offset + 1; //1
			quadIndeces[i + 2] = offset + 0; //2
			
			quadIndeces[i + 3] = offset + 0; //2 
			quadIndeces[i + 4] = offset + 3; //3
			quadIndeces[i + 5] = offset + 2; //0

			offset += 4;
		}

		Ref<IndexBuffer> quadIndexBuffer;
		quadIndexBuffer = IndexBuffer::Create(quadIndeces, s_Data.MaxIndices);
		delete[] quadIndeces;

		BufferLayout squareLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float3, "a_Normal"},
			{ShaderDataType::Float2, "a_TexCoord"},
			{ShaderDataType::Int,	 "a_EntityID"},
			{ShaderDataType::Int,	 "a_DiffuseTextureIndex"},
			{ShaderDataType::Int,	 "a_SpecularTextureIndex"},
			{ShaderDataType::Int,	 "a_NormalTextureIndex"},
			{ShaderDataType::Float,	 "a_TilingFactor"},
			//Material
			{ShaderDataType::Float, "a_MaterialShininess"},
		};
		
		s_Data.QuadVertexBuffer = VertexBuffer::Create(sizeof(QuadVertex) * s_Data.MaxVertices);
		s_Data.QuadVertexBuffer->SetLayout(squareLayout);

		s_Data.QuadVertexArray = VertexArray::Create();
		s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);
		s_Data.QuadVertexArray->SetIndexBuffer(quadIndexBuffer);
		s_Data.QuadVertexArray->Unbind();

		s_Data.QuadVertexBase = new QuadVertex[s_Data.MaxVertices];

		s_Data.NormalsShader = ShaderLibrary::GetOrLoad("assets/shaders/SpriteNormalsShader.glsl");
		s_Data.DirectionalShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/SpriteDirectionalShadowMapShader.glsl");
		s_Data.PointShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/SpritePointShadowMapShader.glsl");
		s_Data.SpriteShader = ShaderLibrary::GetOrLoad("assets/shaders/SpriteShader.glsl");
		InitSpriteShader();
		s_Data.SpriteShader->BindOnReload([]()
		{
			InitSpriteShader();
		});

		s_Data.QuadVertexPosition[0] = {-0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[1] = { 0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[2] = { 0.5f,  0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[3] = {-0.5f,  0.5f, 0.f, 1.f};

		s_Data.QuadVertexNormal[0] = { 0.0f,  0.0f, -1.0f, 0.0f };
		s_Data.QuadVertexNormal[1] = { 0.0f,  0.0f, -1.0f, 0.0f };
		s_Data.QuadVertexNormal[2] = { 0.0f,  0.0f, -1.0f, 0.0f };
		s_Data.QuadVertexNormal[3] = { 0.0f,  0.0f, -1.0f, 0.0f };

		s_Data.BoundTextures.reserve(32);
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBase;
	}

	void Renderer2D::BeginScene(const glm::vec3& cameraPosition, const RenderInfo& renderInfo)
	{
		s_Data.FlushCounter = 0;
		s_Data.bRedrawing = s_Data.bCanRedraw && renderInfo.bRedraw;
		if (renderInfo.drawToShadowMap == DrawToShadowMap::Directional)
			s_Data.CurrentShader = s_Data.DirectionalShadowMapShader;
		else if (renderInfo.drawToShadowMap == DrawToShadowMap::Point)
		{
			s_Data.CurrentShader = s_Data.PointShadowMapShader;
			s_Data.CurrentShader->Bind();
			s_Data.CurrentShader->SetInt("u_PointLightIndex", renderInfo.pointLightIndex);
		}
		else
		{
			s_Data.CurrentShader = s_Data.SpriteShader;
			s_Data.SkyboxShader->Bind();
			s_Data.SkyboxShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);
			s_Data.SkyboxShader->SetFloat("u_Gamma", Renderer::Gamma());
			s_Data.CurrentShader->Bind();
			s_Data.CurrentShader->SetFloat3("u_ViewPos", cameraPosition);
			s_Data.CurrentShader->SetInt("u_SkyboxEnabled", 0);
			s_Data.CurrentShader->SetFloat("u_Gamma", Renderer::Gamma());
		}

		if (s_Data.bRedrawing)
		{
			s_Data.QuadVertexArray->Bind();
			s_Data.QuadVertexBuffer->Bind();
		}
		else
			StartBatch();
	}

	void Renderer2D::InitSpriteShader()
	{
		int32_t samplers[32];
		for (int i = 0; i < 32; ++i)
		{
			samplers[i] = i;
		}
		samplers[0] = 1; //Because 0 - is cubemap (samplerCube)
		samplers[2] = 1; //Because 2 - is cubemap (pointShadowMap)
		samplers[3] = 1; //Because 3 - is cubemap (pointShadowMap)
		samplers[4] = 1; //Because 4 - is cubemap (pointShadowMap)
		samplers[5] = 1; //Because 5 - is cubemap (pointShadowMap)

		int32_t cubeSamplers[4];
		for (int i = 0; i < 4; ++i)
			cubeSamplers[i] = s_Data.PointShadowTextureIndex + i;

		s_Data.SpriteShader->Bind();
		s_Data.SpriteShader->SetIntArray("u_Textures", samplers, sizeof(samplers));
		s_Data.SpriteShader->SetIntArray("u_PointShadowCubemaps", cubeSamplers, MAXPOINTLIGHTS);
		s_Data.SpriteShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);
		s_Data.SpriteShader->SetInt("u_ShadowMap", s_Data.DirectionalShadowTextureIndex);
	}

	void Renderer2D::EndScene()
	{
		Flush();
		DrawCurrentSkybox();
		s_Data.CurrentSkybox.reset();
		s_Data.bCanRedraw = s_Data.FlushCounter == 1;
	}

	void Renderer2D::Flush()
	{
		if (s_Data.IndicesCount == 0)
			return;

		uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexPtr - (uint8_t*)s_Data.QuadVertexBase);
		s_Data.QuadVertexArray->Bind();
		if (!s_Data.bRedrawing)
			s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBase, dataSize);
		s_Data.CurrentShader->Bind();

		for (auto& it : s_Data.BoundTextures)
		{
			it.first->Bind(it.second);
		}

		RenderCommand::DrawIndexed(s_Data.IndicesCount);
		
		bool bDrawingToShadowMap = s_Data.CurrentShader != s_Data.SpriteShader;
		if (!bDrawingToShadowMap && Renderer::IsRenderingNormals())
		{
			s_Data.NormalsShader->Bind();
			RenderCommand::DrawIndexed(s_Data.IndicesCount);
		}

		++s_Data.Stats.DrawCalls;
		++s_Data.FlushCounter;
	}

	bool Renderer2D::IsRedrawing()
	{
		return s_Data.bRedrawing;
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
		s_Data.BoundTextures.clear();
		s_Data.CurrentTextureIndex = s_Data.StartTextureIndex;
		s_Data.BoundTextures[Texture2D::BlackTexture] = s_Data.BlackTextureIndex;

		s_Data.QuadVertexArray->Bind();
		s_Data.QuadVertexBuffer->Bind();
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, material, entityID);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, subtexture, textureProps, entityID);
	}

	void Renderer2D::DrawSkybox(const Ref<Cubemap>& cubemap)
	{
		s_Data.CurrentSkybox = cubemap;
		s_Data.CurrentSkybox->Bind(s_Data.SkyboxTextureIndex);

		s_Data.CurrentShader->Bind();
		s_Data.CurrentShader->SetInt("u_SkyboxEnabled", 1);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		if ((int)s_Data.MaxTexturesIndex - (int)s_Data.CurrentTextureIndex < 3)
			NextBatch();

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };

		uint32_t diffuseTextureIndex = 0;
		uint32_t specularTextureIndex = 0;
		uint32_t normalTextureIndex = 0;

		auto itDiffuse = s_Data.BoundTextures.find(material->DiffuseTexture);
		if (itDiffuse != s_Data.BoundTextures.end())
			diffuseTextureIndex = itDiffuse->second;
		else
		{
			diffuseTextureIndex = s_Data.CurrentTextureIndex++;
			s_Data.BoundTextures[material->DiffuseTexture] = diffuseTextureIndex;
		}

		auto itSpecular = s_Data.BoundTextures.find(material->SpecularTexture);
		if (itSpecular != s_Data.BoundTextures.end())
			specularTextureIndex = itSpecular->second;
		else
		{
			specularTextureIndex = s_Data.CurrentTextureIndex++;
			s_Data.BoundTextures[material->SpecularTexture] = specularTextureIndex;
		}

		auto itNormal = s_Data.BoundTextures.find(material->NormalTexture);
		if (itNormal != s_Data.BoundTextures.end())
			normalTextureIndex = itNormal->second;
		else
		{
			normalTextureIndex = s_Data.CurrentTextureIndex++;
			s_Data.BoundTextures[material->NormalTexture] = normalTextureIndex;
		}

		glm::vec3 myNormal = glm::mat3(glm::transpose(glm::inverse(transform))) * s_Data.QuadVertexNormal[0];

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Normal = myNormal;
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = entityID;
			s_Data.QuadVertexPtr->DiffuseTextureSlotIndex = diffuseTextureIndex;
			s_Data.QuadVertexPtr->SpecularTextureSlotIndex = specularTextureIndex;
			s_Data.QuadVertexPtr->NormalTextureSlotIndex = normalTextureIndex;
			s_Data.QuadVertexPtr->Material = material;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		const glm::vec2* texCoords = subtexture->GetTexCoords();
		const Ref<Texture2D>& texture = subtexture->GetTexture();

		uint32_t textureIndex = 0;

		auto itTexture = s_Data.BoundTextures.find(texture);
		if (itTexture != s_Data.BoundTextures.end())
			textureIndex = itTexture->second;
		else
		{
			if (s_Data.CurrentTextureIndex > s_Data.MaxTexturesIndex)
				NextBatch();
			textureIndex = s_Data.CurrentTextureIndex++;
			s_Data.BoundTextures[texture] = textureIndex;
		}

		glm::vec3 myNormal = glm::mat3(glm::transpose(glm::inverse(transform))) * s_Data.QuadVertexNormal[0];

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Normal = myNormal;
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = entityID;
			s_Data.QuadVertexPtr->DiffuseTextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->SpecularTextureSlotIndex = s_Data.BlackTextureIndex;
			s_Data.QuadVertexPtr->NormalTextureSlotIndex = s_Data.BlackTextureIndex;
			s_Data.QuadVertexPtr->Material.TilingFactor = textureProps.TilingFactor;
			s_Data.QuadVertexPtr->Material.Shininess = 32.f;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::DrawCurrentSkybox()
	{
		if (s_Data.CurrentSkybox)
		{
			RenderCommand::SetDepthFunc(DepthFunc::LEQUAL);
			s_Data.SkyboxVertexArray->Bind();
			s_Data.SkyboxShader->Bind();
			RenderCommand::DrawIndexed(s_Data.SkyboxIndicesCount);
			RenderCommand::SetDepthFunc(DepthFunc::LESS);
			s_Data.SkyboxVertexArray->Unbind();
			++s_Data.Stats.DrawCalls;
		}
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

