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

		RendererMaterial(const Material& material)
		: TilingFactor(material.TilingFactor), Shininess(material.Shininess)
		{}

		RendererMaterial& operator=(const RendererMaterial&) = default;
		RendererMaterial& operator=(const Material& material)
		{
			TilingFactor = material.TilingFactor;
			Shininess = material.Shininess;

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
		RendererMaterial Material;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 10000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxDiffuseTextureSlots = 16;
		static const uint32_t MaxSpecularTextureSlots = 16;
		static const uint32_t SkyboxTextureIndex = 0;
		static const uint32_t StartTextureIndex = 1; //1 - white texture by default, 0 - skybox

		std::array<Ref<Texture>, MaxDiffuseTextureSlots> DiffuseTextureSlots;
		std::array<Ref<Texture>, MaxSpecularTextureSlots> SpecularTextureSlots;

		Ref<Cubemap> CurrentSkybox;
		Ref<VertexArray> QuadVertexArray;
		Ref<VertexBuffer> QuadVertexBuffer;
		Ref<VertexArray> SkyboxVertexArray;
		Ref<VertexBuffer> SkyboxVertexBuffer;
		Ref<Shader> UniqueShader;
		Ref<Shader> SkyboxShader;

		QuadVertex* QuadVertexBase = nullptr;
		QuadVertex* QuadVertexPtr = nullptr;

		uint32_t IndicesCount = 0;
		uint32_t SkyboxIndicesCount = 0;
		uint32_t DiffuseTextureIndex = StartTextureIndex;
		uint32_t SpecularTextureIndex = StartTextureIndex;

		glm::vec4 QuadVertexPosition[4];
		glm::vec4 QuadVertexNormal[4];

		Renderer2D::Statistics Stats;
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

		s_Data.SkyboxShader = Shader::Create("assets/shaders/SkyboxShader.glsl");
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

		s_Data.DiffuseTextureSlots[1] = Texture2D::WhiteTexture;
		s_Data.SpecularTextureSlots[1] = Texture2D::BlackTexture;

		int32_t samplers[32];
		for (int i = 0; i < 32; ++i)
		{
			samplers[i] = i;
		}
		samplers[0] = 1; //Because 0 - is cubemap (samplerCube)

		s_Data.UniqueShader = Shader::Create("assets/shaders/UniqueShader.glsl");
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetIntArray("u_DiffuseTextures", samplers, s_Data.MaxDiffuseTextureSlots);
		s_Data.UniqueShader->SetIntArray("u_SpecularTextures", samplers + s_Data.MaxDiffuseTextureSlots, s_Data.MaxSpecularTextureSlots);

		s_Data.QuadVertexPosition[0] = {-0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[1] = { 0.5f, -0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[2] = { 0.5f,  0.5f, 0.f, 1.f};
		s_Data.QuadVertexPosition[3] = {-0.5f,  0.5f, 0.f, 1.f};

		s_Data.QuadVertexNormal[0] = { 0.0f,  0.0f, -1.0f, 0.f};
		s_Data.QuadVertexNormal[1] = { 0.0f,  0.0f, -1.0f, 0.f};
		s_Data.QuadVertexNormal[2] = { 0.0f,  0.0f, -1.0f, 0.f};
		s_Data.QuadVertexNormal[3] = { 0.0f,  0.0f, -1.0f, 0.f};
	}

	void Renderer2D::Shutdown()
	{
		delete[] s_Data.QuadVertexBase;
	}

	void Renderer2D::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraVP = cameraComponent.GetViewProjection();
		const glm::mat4 cameraProjection = cameraComponent.Camera.GetProjection();
		glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		cameraView = glm::mat4(glm::mat3(cameraView));
		s_Data.SkyboxShader->Bind();
		s_Data.SkyboxShader->SetMat4("u_View", cameraView);
		s_Data.SkyboxShader->SetMat4("u_Projection", cameraProjection);
		s_Data.SkyboxShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetMat4("u_ViewProjection", cameraVP);
		s_Data.UniqueShader->SetFloat3("u_ViewPos", cameraComponent.GetWorldTransform().Translation);
		s_Data.UniqueShader->SetInt("u_SkyboxEnabled", 0);
		s_Data.UniqueShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);

		char uniformTextBuffer[64];
		//PointLight params
		for (int i = 0; i < pointLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Position", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Ambient", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->Ambient);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Diffuse", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Specular", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Distance", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, pointLights[i]->Distance);
		}
		s_Data.UniqueShader->SetInt("u_PointLightsSize", (int)pointLights.size());

		//DirectionalLight params
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Direction", directionalLight.GetForwardDirection());
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Ambient", directionalLight.Ambient);
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Diffuse", directionalLight.LightColor);
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Specular", directionalLight.Specular);

		//SpotLight params
		for (int i = 0; i < spotLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Position", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Direction", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetForwardDirection());
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Diffuse", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Specular", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].InnerCutOffAngle", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, spotLights[i]->InnerCutOffAngle);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].OuterCutOffAngle", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, spotLights[i]->OuterCutOffAngle);
		}
		s_Data.UniqueShader->SetInt("u_SpotLightsSize", (int)spotLights.size());

		StartBatch();
	}

	void Renderer2D::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraVP = editorCamera.GetViewProjection();
		const glm::mat4 cameraProjection = editorCamera.GetProjection();
		glm::mat4 cameraView = editorCamera.GetViewMatrix();
		cameraView = glm::mat4(glm::mat3(cameraView));
		const glm::vec3 cameraPos = editorCamera.GetTranslation();
		s_Data.SkyboxShader->Bind();
		s_Data.SkyboxShader->SetMat4("u_View", cameraView);
		s_Data.SkyboxShader->SetMat4("u_Projection", cameraProjection);
		s_Data.SkyboxShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);
		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetMat4("u_ViewProjection", cameraVP);
		s_Data.UniqueShader->SetFloat3("u_ViewPos", cameraPos);
		s_Data.UniqueShader->SetInt("u_SkyboxEnabled", 0);
		s_Data.UniqueShader->SetInt("u_Skybox", s_Data.SkyboxTextureIndex);

		//PointLight params
		char uniformTextBuffer[64];
		for (int i = 0; i < pointLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Position", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Ambient", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->Ambient);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Diffuse", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Specular", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, pointLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Distance", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, pointLights[i]->Distance);
		}
		s_Data.UniqueShader->SetInt("u_PointLightsSize", (int)pointLights.size());

		//DirectionalLight params
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Direction", directionalLight.GetForwardDirection());
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Ambient", directionalLight.Ambient);
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Diffuse", directionalLight.LightColor);
		s_Data.UniqueShader->SetFloat3("u_DirectionalLight.Specular", directionalLight.Specular);

		//SpotLight params
		for (int i = 0; i < spotLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Position", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Direction", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetForwardDirection());
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Ambient", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->Ambient);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Diffuse", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Specular", i);
			s_Data.UniqueShader->SetFloat3(uniformTextBuffer, spotLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].InnerCutOffAngle", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, spotLights[i]->InnerCutOffAngle);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].OuterCutOffAngle", i);
			s_Data.UniqueShader->SetFloat(uniformTextBuffer, spotLights[i]->OuterCutOffAngle);
		}
		s_Data.UniqueShader->SetInt("u_SpotLightsSize", (int)spotLights.size());

		StartBatch();
	}

	void Renderer2D::EndScene()
	{
		Flush();
		DrawCurrentSkybox();
		s_Data.CurrentSkybox.reset();
	}

	void Renderer2D::Flush()
	{
		if (s_Data.IndicesCount == 0)
			return;

		uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.QuadVertexPtr - (uint8_t*)s_Data.QuadVertexBase);
		s_Data.QuadVertexArray->Bind();
		s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBase, dataSize);
		s_Data.UniqueShader->Bind();

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.DiffuseTextureIndex; ++i)
		{
				s_Data.DiffuseTextureSlots[i]->Bind(i);
		}
		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.SpecularTextureIndex; ++i)
		{
				s_Data.SpecularTextureSlots[i]->Bind(i + s_Data.MaxDiffuseTextureSlots);
		}

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

		s_Data.DiffuseTextureIndex = s_Data.StartTextureIndex + 1;
		s_Data.SpecularTextureIndex = s_Data.StartTextureIndex + 1;

		s_Data.QuadVertexArray->Bind();
		s_Data.QuadVertexBuffer->Bind();
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Material& material, int entityID)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, material, entityID);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, int entityID)
	{
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });

		DrawQuad(transformMatrix, texture, textureProps, entityID);
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

		s_Data.UniqueShader->Bind();
		s_Data.UniqueShader->SetInt("u_SkyboxEnabled", 1);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Material& material, int entityID)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };

		uint32_t diffuseTextureIndex = 0;
		uint32_t specularTextureIndex = 0;

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.DiffuseTextureIndex; ++i)
		{
			if ((*s_Data.DiffuseTextureSlots[i]) == (*material.DiffuseTexture))
			{
				diffuseTextureIndex = i;
				break;
			}
		}
		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.SpecularTextureIndex; ++i)
		{
			if ((*s_Data.SpecularTextureSlots[i]) == (*material.SpecularTexture))
			{
				specularTextureIndex = i;
				break;
			}
		}

		if (diffuseTextureIndex == 0)
		{
			if (s_Data.DiffuseTextureIndex >= Renderer2DData::MaxDiffuseTextureSlots)
				NextBatch();

			diffuseTextureIndex = s_Data.DiffuseTextureIndex;
			s_Data.DiffuseTextureSlots[diffuseTextureIndex] = material.DiffuseTexture;
			++s_Data.DiffuseTextureIndex;
		}
		if (specularTextureIndex == 0)
		{
			if (s_Data.SpecularTextureIndex >= Renderer2DData::MaxSpecularTextureSlots)
				NextBatch();

			specularTextureIndex = s_Data.SpecularTextureIndex;
			s_Data.SpecularTextureSlots[specularTextureIndex] = material.SpecularTexture;
			++s_Data.SpecularTextureIndex;
		}

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Normal = transform * s_Data.QuadVertexNormal[i];
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = entityID;
			s_Data.QuadVertexPtr->DiffuseTextureSlotIndex = diffuseTextureIndex;
			s_Data.QuadVertexPtr->SpecularTextureSlotIndex = specularTextureIndex;
			s_Data.QuadVertexPtr->Material = material;
			++s_Data.QuadVertexPtr;
		}

		s_Data.IndicesCount += 6;

		++s_Data.Stats.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const TextureProps& textureProps, int entityID)
	{
		if (s_Data.IndicesCount >= Renderer2DData::MaxIndices)
			NextBatch();

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };

		uint32_t textureIndex = 0;

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.DiffuseTextureIndex; ++i)
		{
			if ((*s_Data.DiffuseTextureSlots[i]) == (*texture))
			{
				textureIndex = i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (s_Data.DiffuseTextureIndex >= Renderer2DData::MaxDiffuseTextureSlots)
				NextBatch();

			textureIndex = s_Data.DiffuseTextureIndex;
			s_Data.DiffuseTextureSlots[textureIndex] = texture;
			++s_Data.DiffuseTextureIndex;
		}

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Normal = transform * s_Data.QuadVertexNormal[i];
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = entityID;
			s_Data.QuadVertexPtr->DiffuseTextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->SpecularTextureSlotIndex = 1;
			s_Data.QuadVertexPtr->Material.TilingFactor = textureProps.TilingFactor;
			s_Data.QuadVertexPtr->Material.Shininess = 32.f;
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
		const Ref<Texture2D> texture = subtexture->GetTexture();

		uint32_t textureIndex = 0;

		for (uint32_t i = s_Data.StartTextureIndex; i < s_Data.DiffuseTextureIndex; ++i)
		{
			if ((*s_Data.DiffuseTextureSlots[i]) == (*texture))
			{
				textureIndex = i;
				break;
			}
		}

		if (textureIndex == 0)
		{
			if (s_Data.DiffuseTextureIndex >= Renderer2DData::MaxDiffuseTextureSlots)
			{
				NextBatch();
			}

			textureIndex = s_Data.DiffuseTextureIndex;
			s_Data.DiffuseTextureSlots[textureIndex] = texture;
			++s_Data.DiffuseTextureIndex;
		}

		for (int i = 0; i < 4; ++i)
		{
			s_Data.QuadVertexPtr->Position = transform * s_Data.QuadVertexPosition[i];
			s_Data.QuadVertexPtr->Normal = transform * s_Data.QuadVertexNormal[i];
			s_Data.QuadVertexPtr->TexCoord = texCoords[i];
			s_Data.QuadVertexPtr->EntityID = entityID;
			s_Data.QuadVertexPtr->DiffuseTextureSlotIndex = textureIndex;
			s_Data.QuadVertexPtr->SpecularTextureSlotIndex = 1;
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

