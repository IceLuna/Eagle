#include "egpch.h"

#include "Renderer.h"
#include "Renderer2D.h"

#include "Shader.h"
#include "RendererAPI.h"
#include "Framebuffer.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct MyVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec2 TexCoords;
		int Index;

		MyVertex() = default;
		MyVertex(const MyVertex&) = default;

		MyVertex(const Vertex& vertex)
		: Position(vertex.Position)
		, Normal(vertex.Normal)
		, Tangent(vertex.Tangent)
		, TexCoords(vertex.TexCoords)
		, Index(0)
		{}

		MyVertex& operator=(const MyVertex& vertex) = default;

		MyVertex& operator=(const Vertex& vertex)
		{
			Position = vertex.Position;
			Normal = vertex.Normal;
			Tangent = vertex.Tangent;
			TexCoords = vertex.TexCoords;
			return *this;
		}
	};

	struct SpriteData
	{
		const SpriteComponent* Sprite = nullptr;
		int EntityID = -1;
	};

	struct RendererData
	{
		Ref<VertexArray> va;
		Ref<IndexBuffer> ib;
		Ref<VertexBuffer> vb;
		Ref<VertexArray> FinalVA;
		Ref<IndexBuffer> FinalIB;
		Ref<VertexBuffer> FinalVB;
		Ref<Shader> MeshShader;
		Ref<Shader> DirectionalShadowMapShader;
		Ref<Shader> PointShadowMapShader;
		Ref<Shader> SpotShadowMapShader;
		Ref<Shader> GShader;
		Ref<Shader> FinalGShader;
		Ref<Cubemap> Skybox;
		Ref<UniformBuffer> MatricesUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;
		Ref<UniformBuffer> GlobalSettingsUniformBuffer;
		Ref<Framebuffer> FinalFramebuffer;
		Ref<Framebuffer> GFramebuffer;
		Ref<Framebuffer> DirectionalShadowFramebuffer;
		std::array<Ref<Framebuffer>, MAXPOINTLIGHTS> PointShadowFramebuffers;
		std::array<Ref<Framebuffer>, MAXSPOTLIGHTS> SpotShadowFramebuffers;
		glm::mat4 DirectionalLightsVP;
		glm::mat4 OrthoProjection;
		
		std::vector<SpriteData> Sprites;

		Renderer::Statistics Stats;

		glm::vec3 ViewPos;

		uint32_t CurrentPointLightsSize = 0;
		uint32_t CurrentSpotLightsSize = 0;

		static constexpr uint32_t SkyboxTextureIndex = 0;
		static constexpr uint32_t DirectionalShadowTextureIndex = 1;
		static constexpr uint32_t PointShadowTextureIndex = 2; //3, 4, 5
		static constexpr uint32_t SpotShadowTextureIndex = 6; //7, 8, 9
		static constexpr uint32_t GBufferStartTextureIndex = SpotShadowTextureIndex + 4; //7, 8, 9
		static constexpr uint32_t StartTextureIndex = 6;

		static constexpr uint32_t MatricesUniformBufferSize = sizeof(glm::mat4) * 2;
		static constexpr uint32_t PLStructSize = 448, DLStructSize = 128, SLStructSize = 96 + 64, Additional = 8;
		static constexpr uint32_t LightsUniformBufferSize = PLStructSize * MAXPOINTLIGHTS + SLStructSize * MAXPOINTLIGHTS + DLStructSize + Additional;
		static constexpr uint32_t DirectionalShadowMapResolutionMultiplier = 4;
		static constexpr uint32_t ShadowMapSize = 2048;
		static constexpr uint32_t GlobalSettingsUniformBufferSize = 32;
		float Gamma = 2.2f;
		float Exposure = 1.f;
		uint32_t ViewportWidth = 1, ViewportHeight = 1;
	};
	static RendererData s_RendererData;

	struct SMData
	{
		Ref<StaticMesh> StaticMesh;
		Transform WorldTransform;
		int entityID;
	};

	struct BatchData
	{
		static constexpr uint32_t MaxDrawsPerBatch = 15;
		std::array<int, MaxDrawsPerBatch> EntityIDs;
		std::array<glm::mat4, MaxDrawsPerBatch> Models;
		std::array<int, MaxDrawsPerBatch> DiffuseTextures;
		std::array<int, MaxDrawsPerBatch> SpecularTextures;
		std::array<int, MaxDrawsPerBatch> NormalTextures;
		std::array<float, MaxDrawsPerBatch> TilingFactors; //Materiak
		std::array<float, MaxDrawsPerBatch> Shininess; //Material
		std::array<glm::vec4, MaxDrawsPerBatch> TintColors; //Material
		static constexpr uint32_t TextureSlotsAvailable = 31 - s_RendererData.StartTextureIndex - 3; // 3 - Number of textures in material

		std::vector<MyVertex> Vertices;
		std::vector<uint32_t> Indeces;

		std::map<Ref<Shader>, std::vector<SMData>> Meshes;
		std::unordered_map<Ref<Texture>, int> BoundTextures;
		Ref<UniformBuffer> BatchUniformBuffer;

		uint32_t FlushCounter = 0;
		uint32_t CurrentVerticesSize = 0;
		uint32_t CurrentIndecesSize = 0;
		uint32_t AlreadyBatchedVerticesSize = 0;
		uint32_t AlreadyBatchedIndecesSize = 0;
		int CurrentlyDrawingIndex = 0;
		const uint32_t BatchUniformBufferSize = 96 * MaxDrawsPerBatch;
	};
	static BatchData s_BatchData;

	struct {
		bool operator()(const SMData& a, const SMData& b) const 
		{ return glm::length(s_RendererData.ViewPos - a.WorldTransform.Location)
				 < 
				 glm::length(s_RendererData.ViewPos - b.WorldTransform.Location); }
	} customMeshesLess;

	struct {
		bool operator()(const SpriteData& a, const SpriteData& b) const {
			return glm::length(s_RendererData.ViewPos - a.Sprite->GetWorldTransform().Location)
				<
				glm::length(s_RendererData.ViewPos - b.Sprite->GetWorldTransform().Location);
		}
	} customSpritesLess;

	void Renderer::Init()
	{
		RenderCommand::Init();
		Renderer::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000;
		Texture2D::WhiteTexture = Texture2D::Create(1, 1, &whitePixel);
		Texture2D::WhiteTexture->m_Path = "White";
		Texture2D::BlackTexture = Texture2D::Create(1, 1, &blackPixel);
		Texture2D::BlackTexture->m_Path = "Black";
		Texture2D::NoneTexture = Texture2D::Create("assets/textures/Editor/none.png", true, false);
		Texture2D::MeshIconTexture = Texture2D::Create("assets/textures/Editor/meshicon.png", true, false);
		Texture2D::TextureIconTexture = Texture2D::Create("assets/textures/Editor/textureicon.png", true, false);
		Texture2D::SceneIconTexture = Texture2D::Create("assets/textures/Editor/sceneicon.png", true, false);
		Texture2D::FolderIconTexture = Texture2D::Create("assets/textures/Editor/foldericon.png", true, false);
		Texture2D::UnknownIconTexture = Texture2D::Create("assets/textures/Editor/unknownicon.png", true, false);
		Texture2D::PlayButtonTexture = Texture2D::Create("assets/textures/Editor/playbutton.png", true, false);
		Texture2D::StopButtonTexture = Texture2D::Create("assets/textures/Editor/stopbutton.png", true, false);

		s_RendererData.MatricesUniformBuffer = UniformBuffer::Create(s_RendererData.MatricesUniformBufferSize, 0);
		s_RendererData.LightsUniformBuffer = UniformBuffer::Create(s_RendererData.LightsUniformBufferSize, 1);
		s_RendererData.GlobalSettingsUniformBuffer = UniformBuffer::Create(s_RendererData.GlobalSettingsUniformBufferSize, 3);

		//Renderer3D Init
		FramebufferSpecification gFbSpecs;
		FramebufferSpecification finalFbSpecs;
		FramebufferSpecification directionalShadowFbSpecs;
		FramebufferSpecification pointShadowFbSpecs;
		FramebufferSpecification spotShadowFbSpecs;
		finalFbSpecs.Width = gFbSpecs.Width = directionalShadowFbSpecs.Width = 1; //1 for now. After window will be launched, it'll updated viewport's size and so framebuffer will be resized.
		finalFbSpecs.Height = gFbSpecs.Height = directionalShadowFbSpecs.Height = 1; //1 for now. After window will be launched, it'll updated viewport's size and so framebuffer will be resized.
		gFbSpecs.Attachments = { {FramebufferTextureFormat::RGB32F}, {FramebufferTextureFormat::RGB32F}, {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RGB16F}, {FramebufferTextureFormat::RED_INTEGER}, {FramebufferTextureFormat::DEPTH24STENCIL8} };
		finalFbSpecs.Attachments = { {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::DEPTH24STENCIL8} };
		directionalShadowFbSpecs.Attachments = { {FramebufferTextureFormat::DEPTH32F} };

		spotShadowFbSpecs.Width = pointShadowFbSpecs.Width = s_RendererData.ShadowMapSize;
		spotShadowFbSpecs.Height = pointShadowFbSpecs.Height = s_RendererData.ShadowMapSize;
		pointShadowFbSpecs.Attachments = { {FramebufferTextureFormat::DEPTH32F, true} };
		spotShadowFbSpecs.Attachments = { {FramebufferTextureFormat::DEPTH32F} };

		s_RendererData.GFramebuffer = Framebuffer::Create(gFbSpecs);
		s_RendererData.FinalFramebuffer = Framebuffer::Create(finalFbSpecs);
		s_RendererData.DirectionalShadowFramebuffer = Framebuffer::Create(directionalShadowFbSpecs);
		
		for (auto& framebuffer : s_RendererData.PointShadowFramebuffers)
			framebuffer = Framebuffer::Create(pointShadowFbSpecs);

		for (auto& framebuffer : s_RendererData.SpotShadowFramebuffers)
			framebuffer = Framebuffer::Create(spotShadowFbSpecs);

		s_RendererData.FinalGShader = ShaderLibrary::GetOrLoad("assets/shaders/FinalGShader.glsl");
		InitFinalGShader();
		s_RendererData.FinalGShader->BindOnReload(&Renderer::InitFinalGShader);

		s_RendererData.GShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshGShader.glsl");
		InitGShader();
		s_RendererData.GShader->BindOnReload(&Renderer::InitGShader);

		s_RendererData.MeshShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshShader.glsl");
		s_RendererData.MeshShader->BindOnReload(&Renderer::InitMeshShader);
		InitMeshShader();

		s_RendererData.DirectionalShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshDirectionalShadowMapShader.glsl");
		s_RendererData.PointShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshPointShadowMapShader.glsl");
		s_RendererData.SpotShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshSpotShadowMapShader.glsl");

		BufferLayout bufferLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float3, "a_Normal"},
			{ShaderDataType::Float3, "a_Tangent"},
			{ShaderDataType::Float2, "a_TexCoord"},
			{ShaderDataType::Int, "a_Index"},
		};

		s_RendererData.ib = IndexBuffer::Create();
		s_RendererData.vb = VertexBuffer::Create();
		s_RendererData.vb->SetLayout(bufferLayout);

		s_RendererData.va = VertexArray::Create();
		s_RendererData.va->AddVertexBuffer(s_RendererData.vb);
		s_RendererData.va->SetIndexBuffer(s_RendererData.ib);
		s_RendererData.va->Unbind();

		s_RendererData.Sprites.reserve(1'000);
		s_BatchData.Vertices.reserve(1'000'000);
		s_BatchData.Indeces.reserve(1'000'000);
		s_BatchData.BatchUniformBuffer = UniformBuffer::Create(s_BatchData.BatchUniformBufferSize, 2);
		s_BatchData.DiffuseTextures.fill(1);
		s_BatchData.SpecularTextures.fill(1);
		s_BatchData.NormalTextures.fill(1);

		//uint32_t size = s_RendererData.LightsUniformBuffer->GetBlockSize("Lights", s_RendererData.FinalGShader); //2576

		InitFinalRenderingBuffers();

		//Renderer2D Init
		Renderer2D::Init();
	}

	void Renderer::InitMeshShader()
	{
		s_RendererData.MeshShader->Bind();
		std::array<int, 32> samplers;
		samplers.fill(1);

		for (int i = s_RendererData.StartTextureIndex; i < samplers.size(); ++i)
			samplers[i] = i;

		s_RendererData.MeshShader->SetIntArray("u_Textures", samplers.data(), (uint32_t)samplers.size());
	}

	void Renderer::InitGShader()
	{
		s_RendererData.GShader->Bind();
		std::array<int, 32> samplers;
		samplers.fill(1);
		for (int i = s_RendererData.StartTextureIndex; i < samplers.size(); ++i)
			samplers[i] = i;

		s_RendererData.GShader->SetIntArray("u_Textures", samplers.data(), (uint32_t)samplers.size());
	}

	void Renderer::InitFinalGShader()
	{
		s_RendererData.FinalGShader->Bind();

		s_RendererData.FinalGShader->SetInt("u_PositionTexture", s_RendererData.GBufferStartTextureIndex);
		s_RendererData.FinalGShader->SetInt("u_NormalsTexture", s_RendererData.GBufferStartTextureIndex + 1);
		s_RendererData.FinalGShader->SetInt("u_AlbedoTexture", s_RendererData.GBufferStartTextureIndex + 2);
		s_RendererData.FinalGShader->SetInt("u_MaterialTexture", s_RendererData.GBufferStartTextureIndex + 3);
	}

	void Renderer::InitFinalRenderingBuffers()
	{
		BufferLayout bufferLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float2, "a_TexCoord"}
		};

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f } };
		constexpr glm::vec3 quadVertexPosition[4] = { { -1.f, -1.f, 0.f }, { 1.f, -1.f, 0.f }, { 1.f,  1.f, 0.f }, { -1.f,  1.f, 0.f } };

		constexpr uint32_t bufferSize = sizeof(glm::vec3) * 4 + sizeof(glm::vec2) * 4;
		uint8_t buffer[bufferSize];
		uint32_t offset = 0;

		for (int i = 0; i < 4; ++i)
		{
			memcpy_s(buffer + offset, bufferSize, &quadVertexPosition[i].x, sizeof(glm::vec3));
			offset += sizeof(glm::vec3);
			memcpy_s(buffer + offset, bufferSize, &texCoords[i].x, sizeof(glm::vec2));
			offset += sizeof(glm::vec2);
		}

		//constexpr uint32_t indeces[] = { 2, 1, 0, 0, 3, 2 };
		constexpr uint32_t indeces[] = { 0, 1, 2, 2, 3, 0 };

		s_RendererData.FinalIB = IndexBuffer::Create(indeces, 6);
		s_RendererData.FinalVB = VertexBuffer::Create(buffer, bufferSize);
		s_RendererData.FinalVB->SetLayout(bufferLayout);

		s_RendererData.FinalVA = VertexArray::Create();
		s_RendererData.FinalVA->AddVertexBuffer(s_RendererData.FinalVB);
		s_RendererData.FinalVA->SetIndexBuffer(s_RendererData.FinalIB);
		s_RendererData.FinalVA->Unbind();
	}

	void Renderer::PrepareRendering()
	{
		s_RendererData.GFramebuffer->Bind();
		Renderer::Clear();
		s_RendererData.GFramebuffer->ClearColorAttachment(4, -1); //4 - RED_INTEGER

		Renderer::ResetStats();
		Renderer2D::ResetStats();
	}

	void Renderer::FinishRendering()
	{
		s_RendererData.FinalFramebuffer->Unbind();
		s_RendererData.Skybox.reset();
		s_RendererData.Sprites.clear();
		s_BatchData.Meshes.clear();
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::PrepareRendering();
		const glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();

		s_RendererData.CurrentPointLightsSize = (uint32_t)pointLights.size();
		s_RendererData.CurrentSpotLightsSize = (uint32_t)spotLights.size();
		s_RendererData.ViewPos = cameraComponent.GetWorldTransform().Location;
		const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Location;
		s_RendererData.DirectionalLightsVP = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardDirection(), glm::vec3(0.f, 1.f, 0.f));
		s_RendererData.DirectionalLightsVP = s_RendererData.OrthoProjection * s_RendererData.DirectionalLightsVP;

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
		SetupGlobalSettingsUniforms();
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::PrepareRendering();
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();

		s_RendererData.CurrentPointLightsSize = (uint32_t)pointLights.size();
		s_RendererData.CurrentSpotLightsSize = (uint32_t)spotLights.size();
		s_RendererData.ViewPos = editorCamera.GetLocation();
		const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Location;
		s_RendererData.DirectionalLightsVP = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardDirection(), glm::vec3(0.f, 1.f, 0.f));
		s_RendererData.DirectionalLightsVP = s_RendererData.OrthoProjection * s_RendererData.DirectionalLightsVP;
		
		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
		SetupGlobalSettingsUniforms();
	}

	void Renderer::StartBatch()
	{
		s_BatchData.CurrentlyDrawingIndex = 0;
		s_BatchData.BoundTextures.clear();

		s_BatchData.AlreadyBatchedVerticesSize += s_BatchData.CurrentVerticesSize;
		s_BatchData.AlreadyBatchedIndecesSize += s_BatchData.CurrentIndecesSize;
		s_BatchData.CurrentVerticesSize = 0;
		s_BatchData.CurrentIndecesSize = 0;
	}

	void Renderer::SetupLightUniforms(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const uint32_t pointLightsSize = (uint32_t)pointLights.size();
		const uint32_t spotLightsSize = (uint32_t)spotLights.size();
		const uint32_t uniformBufferSize = s_RendererData.LightsUniformBufferSize;

		uint8_t* uniformBuffer = new uint8_t[uniformBufferSize];

		//DirectionalLight params
		glm::vec3 dirLightForwardVector = directionalLight.GetForwardDirection();

		uint32_t ubOffset = 0;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &s_RendererData.DirectionalLightsVP[0][0], sizeof(glm::mat4));
		ubOffset += 64;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &dirLightForwardVector[0], sizeof(glm::vec3));
		ubOffset += 16;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &directionalLight.Ambient[0], sizeof(glm::vec3));
		ubOffset += 16;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &directionalLight.LightColor[0], sizeof(glm::vec3));
		ubOffset += 16;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &directionalLight.Specular[0], sizeof(glm::vec3));
		ubOffset += 16;

		//SpotLight params
		constexpr float shadowMapAspectRatio = (float)s_RendererData.ShadowMapSize / (float)s_RendererData.ShadowMapSize;

		for (uint32_t i = 0; i < spotLightsSize; ++i)
		{
			const glm::vec3& spotLightLocation = spotLights[i]->GetWorldTransform().Location;
			const glm::vec3 spotLightForwardVector = spotLights[i]->GetForwardDirection();

			glm::mat4 spotLightPerspectiveProjection = glm::perspective(glm::radians(glm::min(spotLights[i]->OuterCutOffAngle * 2.f, 179.f)), shadowMapAspectRatio, 0.01f, 10000.f);

			const glm::mat4 VP = spotLightPerspectiveProjection * glm::lookAt(spotLightLocation, spotLightLocation + spotLightForwardVector, glm::vec3{0.f, 1.f, 0.f});

			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &VP[0][0], sizeof(glm::mat4));
			ubOffset += sizeof(glm::mat4);
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLightLocation[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLightForwardVector[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->Ambient[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->LightColor[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->Specular[0], sizeof(glm::vec3));
			ubOffset += 12;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->InnerCutOffAngle, sizeof(float));
			ubOffset += 4;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->OuterCutOffAngle, sizeof(float));
			ubOffset += 4;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLights[i]->Intensity, sizeof(float));
			ubOffset += 12;
		}
		ubOffset += (MAXSPOTLIGHTS - spotLightsSize) * s_RendererData.SLStructSize; //If not all spot lights used, add additional offset

		//Params of Depth Cubemap
		glm::mat4 pointLightVPs[6];
		constexpr glm::vec3 directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
									glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),  glm::vec3(0.0, 0.0,-1.0) };
		constexpr glm::vec3 upVectors[6] = { glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),
											 glm::vec3(0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0) };
		//PointLight params
		const glm::mat4 pointLightPerspectiveProjection = glm::perspective(glm::radians(90.f), shadowMapAspectRatio, 0.01f, 10000.f);
		
		for (uint32_t i = 0; i < pointLightsSize; ++i)
		{
			const glm::vec3& pointLightsLocation = pointLights[i]->GetWorldTransform().Location;

			for (int j = 0; j < 6; ++j)
				pointLightVPs[j] = pointLightPerspectiveProjection * glm::lookAt(pointLightsLocation, pointLightsLocation + directions[j], upVectors[j]);

			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLightVPs[0][0], sizeof(glm::mat4) * 6);
			ubOffset += sizeof(glm::mat4) * 6;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLightsLocation[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Ambient[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->LightColor[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Specular[0], sizeof(glm::vec3));
			ubOffset += 12;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Intensity, sizeof(float));
			ubOffset += 4;
		}
		ubOffset += (MAXPOINTLIGHTS - pointLightsSize) * s_RendererData.PLStructSize; //If not all point lights used, add additional offset

		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLightsSize, sizeof(uint32_t));
		ubOffset += 4;
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLightsSize, sizeof(uint32_t));
		ubOffset += 4;

		s_RendererData.LightsUniformBuffer->Bind();
		s_RendererData.LightsUniformBuffer->UpdateData(uniformBuffer, uniformBufferSize, 0);

		delete[] uniformBuffer;
	}

	void Renderer::SetupMatricesUniforms(const glm::mat4& view, const glm::mat4& projection)
	{
		const uint32_t bufferSize = s_RendererData.MatricesUniformBufferSize;
		uint8_t * buffer = new uint8_t[bufferSize];
		memcpy_s(buffer, bufferSize, &view[0][0], sizeof(glm::mat4));
		memcpy_s(buffer + sizeof(glm::mat4), bufferSize, &projection[0][0], sizeof(glm::mat4));
		s_RendererData.MatricesUniformBuffer->Bind();
		s_RendererData.MatricesUniformBuffer->UpdateData(buffer, bufferSize, 0);
		delete[] buffer;
	}

	void Renderer::SetupGlobalSettingsUniforms()
	{
		const uint32_t bufferSize = s_RendererData.GlobalSettingsUniformBufferSize;
		uint8_t* buffer = new uint8_t[bufferSize];
		uint32_t offset = 0;
		memcpy_s(buffer, bufferSize, &s_RendererData.ViewPos, sizeof(glm::vec3));
		offset += 12;
		memcpy_s(buffer + offset, bufferSize, &s_RendererData.Gamma, sizeof(float));
		offset += 4;
		memcpy_s(buffer + offset, bufferSize, &s_RendererData.Exposure, sizeof(float));
		s_RendererData.GlobalSettingsUniformBuffer->Bind();
		s_RendererData.GlobalSettingsUniformBuffer->UpdateData(buffer, bufferSize, 0);
		delete[] buffer;
	}

	void Renderer::DrawSkybox(const Ref<Cubemap>& cubemap)
	{
		s_RendererData.Skybox = cubemap;
		s_RendererData.Skybox->Bind(s_RendererData.SkyboxTextureIndex);
	}

	void Renderer::EndScene()
	{
		//Sorting from front to back
		std::sort(std::begin(s_RendererData.Sprites), std::end(s_RendererData.Sprites), customSpritesLess);
		for (auto& mesh : s_BatchData.Meshes)
		{
			auto& meshes = mesh.second;
			std::sort(std::begin(meshes), std::end(meshes), customMeshesLess);
		}

		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

		GBufferPass();
		Renderer2D::Statistics renderer2DStats = Renderer2D::GetStats(); //Saving 2D stats

		ShadowPass();

		Renderer::ResetStats();
		FinalPass();

		Renderer2D::GetStats() = renderer2DStats; //Restoring 2D stats

		Renderer::FinishRendering();

		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		s_RendererData.Stats.RenderingTook = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;
	}

	void Renderer::FinalPass()
	{
		//Rendering to Color Attachments
		RenderCommand::SetViewport(0, 0, s_RendererData.ViewportWidth, s_RendererData.ViewportHeight);
		s_RendererData.FinalFramebuffer->Bind();
		Renderer::Clear();
		s_RendererData.DirectionalShadowFramebuffer->BindDepthTexture(s_RendererData.DirectionalShadowTextureIndex, 0);

		for (uint32_t i = 0; i < s_RendererData.CurrentPointLightsSize; ++i)
			s_RendererData.PointShadowFramebuffers[i]->BindDepthTexture(s_RendererData.PointShadowTextureIndex + i, 0);

		for (uint32_t i = 0; i < s_RendererData.CurrentSpotLightsSize; ++i)
			s_RendererData.SpotShadowFramebuffers[i]->BindDepthTexture(s_RendererData.SpotShadowTextureIndex + i, 0);

		s_RendererData.GFramebuffer->BindColorTexture(s_RendererData.GBufferStartTextureIndex, 0);
		s_RendererData.GFramebuffer->BindColorTexture(s_RendererData.GBufferStartTextureIndex + 1, 1);
		s_RendererData.GFramebuffer->BindColorTexture(s_RendererData.GBufferStartTextureIndex + 2, 2);
		s_RendererData.GFramebuffer->BindColorTexture(s_RendererData.GBufferStartTextureIndex + 3, 3);
		FinalDrawUsingGBuffer();
	}

	void Renderer::GBufferPass()
	{
		RenderInfo gBufferRenderInfo = { DrawTo::GBuffer, -1, false };

		RenderCommand::SetViewport(0, 0, s_RendererData.ViewportWidth, s_RendererData.ViewportHeight);
		s_RendererData.GFramebuffer->Bind();

		DrawPassedMeshes(gBufferRenderInfo);
		DrawPassedSprites(gBufferRenderInfo);
	}

	void Renderer::ShadowPass()
	{
		RenderInfo directionalLightRenderInfo = { DrawTo::DirectionalShadowMap, -1, true };
		RenderInfo pointLightRenderInfo = { DrawTo::PointShadowMap, -1, true };
		RenderInfo spotLightRenderInfo = { DrawTo::SpotShadowMap, -1, true };

		//Rendering to ShadowMap
		RenderCommand::SetViewport(0, 0, s_RendererData.ViewportWidth * s_RendererData.DirectionalShadowMapResolutionMultiplier, s_RendererData.ViewportHeight * s_RendererData.DirectionalShadowMapResolutionMultiplier);
		s_RendererData.DirectionalShadowFramebuffer->Bind();
		RenderCommand::ClearDepthBuffer();
		DrawPassedMeshes(directionalLightRenderInfo);
		DrawPassedSprites(directionalLightRenderInfo);

		RenderCommand::SetViewport(0, 0, s_RendererData.ShadowMapSize, s_RendererData.ShadowMapSize);
		for (uint32_t i = 0; i < s_RendererData.CurrentPointLightsSize; ++i)
		{
			pointLightRenderInfo.pointLightIndex = i;
			s_RendererData.PointShadowFramebuffers[i]->Bind();
			RenderCommand::ClearDepthBuffer();
			//Rendering to ShadowMap
			DrawPassedMeshes(pointLightRenderInfo);
			DrawPassedSprites(pointLightRenderInfo);
		}

		for (uint32_t i = 0; i < s_RendererData.CurrentSpotLightsSize; ++i)
		{
			spotLightRenderInfo.pointLightIndex = i;
			s_RendererData.SpotShadowFramebuffers[i]->Bind();
			RenderCommand::ClearDepthBuffer();
			//Rendering to ShadowMap
			DrawPassedMeshes(spotLightRenderInfo);
			DrawPassedSprites(spotLightRenderInfo);
		}
	}

	void Renderer::FinalDrawUsingGBuffer()
	{
		s_RendererData.FinalVA->Bind();
		s_RendererData.FinalGShader->Bind();

		int samplers[4];
		for (int i = 0; i < 4; ++i)
			samplers[i] = s_RendererData.PointShadowTextureIndex + i;

		s_RendererData.FinalGShader->SetIntArray("u_PointShadowCubemaps", samplers, MAXPOINTLIGHTS);

		bool bSkybox = s_RendererData.Skybox.operator bool();

		for (int i = 0; i < 4; ++i)
			samplers[i] = s_RendererData.SpotShadowTextureIndex + i;

		s_RendererData.FinalGShader->SetIntArray("u_SpotShadowMaps", samplers, MAXSPOTLIGHTS);

		s_RendererData.FinalGShader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);
		s_RendererData.FinalGShader->SetInt("u_SkyboxEnabled", int(bSkybox));
		s_RendererData.FinalGShader->SetInt("u_ShadowMap", s_RendererData.DirectionalShadowTextureIndex);

		constexpr uint32_t count = 6; //s_RendererData.FinalVA->GetIndexBuffer()->GetCount();
		RenderCommand::DrawIndexed(count);

		if (s_RendererData.Skybox)
		{
			s_RendererData.FinalFramebuffer->CopyDepthBufferFrom(s_RendererData.GFramebuffer);
			Renderer2D::BeginScene({ DrawTo::None, -1, false });
			Renderer2D::DrawSkybox(s_RendererData.Skybox);
			Renderer2D::EndScene();
		}
	}

	void Renderer::DrawPassedMeshes(const RenderInfo& renderInfo)
	{
		s_BatchData.AlreadyBatchedVerticesSize = s_BatchData.AlreadyBatchedIndecesSize = 0;
		s_BatchData.CurrentVerticesSize = s_BatchData.CurrentIndecesSize = 0;

		const Ref<Shader>* shadowShader = nullptr;
		bool bDrawToShadowMap = (renderInfo.drawTo == DrawTo::PointShadowMap) || (renderInfo.drawTo == DrawTo::DirectionalShadowMap) || (renderInfo.drawTo == DrawTo::SpotShadowMap);
		if (renderInfo.drawTo == DrawTo::DirectionalShadowMap)
			shadowShader = &s_RendererData.DirectionalShadowMapShader;
		else if (renderInfo.drawTo == DrawTo::PointShadowMap)
		{
			shadowShader = &s_RendererData.PointShadowMapShader;
			(*shadowShader)->Bind();
			(*shadowShader)->SetInt("u_PointLightIndex", renderInfo.pointLightIndex);
		}
		else if (renderInfo.drawTo == DrawTo::SpotShadowMap)
		{
			shadowShader = &s_RendererData.SpotShadowMapShader;
			(*shadowShader)->Bind();
			(*shadowShader)->SetInt("u_SpotLightIndex", renderInfo.pointLightIndex);
		}

		bool bFastRedraw = false;
		if (!renderInfo.bRedraw)
		{
			s_BatchData.Vertices.clear();
			s_BatchData.Indeces.clear();
		}
		else
			bFastRedraw = (s_BatchData.FlushCounter == 1);

		s_BatchData.FlushCounter = 0;

		for (auto it : s_BatchData.Meshes)
		{
			const Ref<Shader>& shader = renderInfo.drawTo == DrawTo::GBuffer ? s_RendererData.GShader : it.first;
			const std::vector<SMData>& smData = it.second;
			uint32_t textureIndex = s_RendererData.StartTextureIndex;
			StartBatch();

			for (auto& sm : smData)
			{
				const int entityID = sm.entityID;

				auto& diffuseTexture = sm.StaticMesh->Material->DiffuseTexture;
				auto& specularTexture = sm.StaticMesh->Material->SpecularTexture;
				auto& normalTexture = sm.StaticMesh->Material->NormalTexture;
				auto itDiffuse = s_BatchData.BoundTextures.find(diffuseTexture);
				auto itSpecular = s_BatchData.BoundTextures.find(specularTexture);
				auto itNormal = s_BatchData.BoundTextures.find(normalTexture);
				int diffuseTextureSlot = 0;
				int specularTextureSlot = 0;
				int normalTextureSlot = 0;

				if (s_BatchData.BoundTextures.size() > s_BatchData.TextureSlotsAvailable)
				{
					if (itDiffuse == s_BatchData.BoundTextures.end()
						|| itSpecular == s_BatchData.BoundTextures.end()
						|| itNormal == s_BatchData.BoundTextures.end())
					{
						FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, renderInfo.drawTo, renderInfo.bRedraw);
						StartBatch();
						itDiffuse = itSpecular = itNormal = s_BatchData.BoundTextures.end();
					}
				}

				bool bRepeatSearch = false;
				if (diffuseTexture)
				{
					if (itDiffuse == s_BatchData.BoundTextures.end())
					{
						diffuseTextureSlot = textureIndex++;
						diffuseTexture->Bind(diffuseTextureSlot);
						s_BatchData.BoundTextures[diffuseTexture] = diffuseTextureSlot;
						bRepeatSearch = true;
					}
					else
					{
						diffuseTextureSlot = itDiffuse->second;
					}
				}
				else
					diffuseTextureSlot = -1;

				if (specularTexture)
				{
					if (bRepeatSearch)
						itSpecular = s_BatchData.BoundTextures.find(specularTexture);
					if (itSpecular == s_BatchData.BoundTextures.end())
					{
						specularTextureSlot = textureIndex++;
						specularTexture->Bind(specularTextureSlot);
						s_BatchData.BoundTextures[specularTexture] = specularTextureSlot;
						bRepeatSearch = true;
					}
					else
					{
						specularTextureSlot = itSpecular->second;
					}
				}
				else
					specularTextureSlot = -1;

				if (normalTexture)
				{
					if (bRepeatSearch)
						itNormal = s_BatchData.BoundTextures.find(normalTexture);
					if (itNormal == s_BatchData.BoundTextures.end())
					{
						normalTextureSlot = textureIndex++;
						normalTexture->Bind(normalTextureSlot);
						s_BatchData.BoundTextures[normalTexture] = normalTextureSlot;
						bRepeatSearch = true;
					}
					else
					{
						normalTextureSlot = itNormal->second;
					}
				}
				else
					normalTextureSlot = -1;

				const std::vector<Vertex>& vertices = sm.StaticMesh->GetVertices();
				const std::vector<uint32_t>& indeces = sm.StaticMesh->GetIndeces();
				if (renderInfo.bRedraw)
				{
					s_BatchData.CurrentVerticesSize += (uint32_t)vertices.size();
					s_BatchData.CurrentIndecesSize += (uint32_t)indeces.size();
				}
				else
				{
					const size_t vSizeBeforeCopy = s_BatchData.Vertices.size();
					s_BatchData.Vertices.insert(std::end(s_BatchData.Vertices), std::begin(vertices), std::end(vertices));
					const size_t vSizeAfterCopy = s_BatchData.Vertices.size();

					for (size_t i = vSizeBeforeCopy; i < vSizeAfterCopy; ++i)
						s_BatchData.Vertices[i].Index = s_BatchData.CurrentlyDrawingIndex;

					const size_t iSizeBeforeCopy = s_BatchData.Indeces.size();
					s_BatchData.Indeces.insert(std::end(s_BatchData.Indeces), std::begin(indeces), std::end(indeces));
					const size_t iSizeAfterCopy = s_BatchData.Indeces.size();

					for (size_t i = iSizeBeforeCopy; i < iSizeAfterCopy; ++i)
						s_BatchData.Indeces[i] += (uint32_t)vSizeBeforeCopy;
				}

				if (!bFastRedraw)
				{
					const Transform& transform = sm.WorldTransform;
					glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

					s_BatchData.EntityIDs[s_BatchData.CurrentlyDrawingIndex] = entityID;
					s_BatchData.Models[s_BatchData.CurrentlyDrawingIndex] = transformMatrix;
					s_BatchData.DiffuseTextures[s_BatchData.CurrentlyDrawingIndex] = diffuseTextureSlot;
					s_BatchData.SpecularTextures[s_BatchData.CurrentlyDrawingIndex] = specularTextureSlot;
					s_BatchData.NormalTextures[s_BatchData.CurrentlyDrawingIndex] = normalTextureSlot;
					s_BatchData.Shininess[s_BatchData.CurrentlyDrawingIndex] = sm.StaticMesh->Material->Shininess;
					s_BatchData.TilingFactors[s_BatchData.CurrentlyDrawingIndex] = sm.StaticMesh->Material->TilingFactor;
					s_BatchData.TintColors[s_BatchData.CurrentlyDrawingIndex] = sm.StaticMesh->Material->TintColor;
				}

				++s_BatchData.CurrentlyDrawingIndex;

				if (s_BatchData.CurrentlyDrawingIndex == s_BatchData.MaxDrawsPerBatch)
				{
					FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, renderInfo.drawTo, renderInfo.bRedraw);
					StartBatch();
				}
			}

			if (s_BatchData.CurrentlyDrawingIndex)
				FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, renderInfo.drawTo, renderInfo.bRedraw);
		}
	}

	void Renderer::DrawPassedSprites(const RenderInfo& renderInfo)
	{
		bool bFinalDraw = (renderInfo.drawTo == DrawTo::None);
		Renderer2D::BeginScene(renderInfo);
		if (bFinalDraw && s_RendererData.Skybox)
			Renderer2D::DrawSkybox(s_RendererData.Skybox);
		if (!Renderer2D::IsRedrawing())
		{
			for (auto& spriteData : s_RendererData.Sprites)
			{
				const SpriteComponent* sprite = spriteData.Sprite;
				const int entityID = spriteData.EntityID;
				const auto& material = sprite->Material;

				if (sprite->bSubTexture)
					Renderer2D::DrawQuad(sprite->GetWorldTransform(), sprite->SubTexture, { material->TintColor, 1.f, material->TilingFactor, material->Shininess }, (int)entityID);
				else
					Renderer2D::DrawQuad(sprite->GetWorldTransform(), material, (int)entityID);
			}
		}
		Renderer2D::EndScene();
	}

	void Renderer::FlushMeshes(const Ref<Shader>& shader, DrawTo drawTo, bool bRedrawing)
	{
		if (s_BatchData.CurrentlyDrawingIndex == 0)
			return;

		uint32_t verticesCount = 0;
		uint32_t indecesCount = 0;
		uint32_t alreadyBatchedVerticesSize = 0;
		uint32_t alreadyBatchedIndecesSize = 0;

		if (bRedrawing)
		{
			verticesCount = s_BatchData.CurrentVerticesSize;
			indecesCount = s_BatchData.CurrentIndecesSize;
			alreadyBatchedVerticesSize = s_BatchData.AlreadyBatchedVerticesSize;
			alreadyBatchedIndecesSize = s_BatchData.AlreadyBatchedIndecesSize;
		}
		else
		{
			verticesCount = (uint32_t)s_BatchData.Vertices.size();
			indecesCount = (uint32_t)s_BatchData.Indeces.size();
		}
		const uint32_t bufferSize = s_BatchData.BatchUniformBufferSize;
		uint8_t* buffer = new uint8_t[bufferSize];
		memcpy_s(buffer, bufferSize, s_BatchData.Models.data(), s_BatchData.CurrentlyDrawingIndex * sizeof(glm::mat4));

		uint32_t offset = s_BatchData.MaxDrawsPerBatch * sizeof(glm::mat4);

		for (int i = 0; i < s_BatchData.CurrentlyDrawingIndex; ++i)
		{
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.TintColors[i], sizeof(glm::vec4));
			offset += sizeof(glm::vec4);
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.EntityIDs[i], sizeof(int));
			offset += sizeof(int);
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.TilingFactors[i], sizeof(float));
			offset += sizeof(float);
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.Shininess[i], sizeof(float));
			offset += 8;
		}

		offset += (s_BatchData.MaxDrawsPerBatch - s_BatchData.CurrentlyDrawingIndex) * 16;

		s_BatchData.BatchUniformBuffer->Bind();
		s_BatchData.BatchUniformBuffer->UpdateData(buffer, bufferSize, 0);
		delete[] buffer;

		shader->Bind();
		if (drawTo == DrawTo::None)
		{
			int samplers[4];
			for (int i = 0; i < 4; ++i)
				samplers[i] = s_RendererData.PointShadowTextureIndex + i;

			shader->SetIntArray("u_PointShadowCubemaps", samplers, MAXPOINTLIGHTS);

			for (int i = 0; i < 4; ++i)
				samplers[i] = s_RendererData.SpotShadowTextureIndex + i;

			s_RendererData.FinalGShader->SetIntArray("u_SpotShadowMaps", samplers, MAXSPOTLIGHTS);

			shader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);
			shader->SetInt("u_ShadowMap", s_RendererData.DirectionalShadowTextureIndex);
			bool bSkybox = s_RendererData.Skybox.operator bool();
			shader->SetInt("u_SkyboxEnabled", int(bSkybox));

			shader->SetIntArray("u_DiffuseTextureSlotIndexes", s_BatchData.DiffuseTextures.data(), (uint32_t)s_BatchData.DiffuseTextures.size());
			shader->SetIntArray("u_SpecularTextureSlotIndexes", s_BatchData.SpecularTextures.data(), (uint32_t)s_BatchData.SpecularTextures.size());
			shader->SetIntArray("u_NormalTextureSlotIndexes", s_BatchData.NormalTextures.data(), (uint32_t)s_BatchData.NormalTextures.size());
		}
		else if (drawTo == DrawTo::GBuffer)
		{
			shader->SetIntArray("u_DiffuseTextureSlotIndexes", s_BatchData.DiffuseTextures.data(), (uint32_t)s_BatchData.DiffuseTextures.size());
			shader->SetIntArray("u_SpecularTextureSlotIndexes", s_BatchData.SpecularTextures.data(), (uint32_t)s_BatchData.SpecularTextures.size());
			shader->SetIntArray("u_NormalTextureSlotIndexes", s_BatchData.NormalTextures.data(), (uint32_t)s_BatchData.NormalTextures.size());
		}

		s_RendererData.va->Bind();
		s_RendererData.ib->Bind();
		s_RendererData.ib->SetData(s_BatchData.Indeces.data() + alreadyBatchedIndecesSize, indecesCount);
		s_RendererData.vb->Bind();
		s_RendererData.vb->SetData(s_BatchData.Vertices.data() + alreadyBatchedVerticesSize, sizeof(MyVertex) * verticesCount);

		RenderCommand::DrawIndexed(indecesCount);

		++s_BatchData.FlushCounter;
		++s_RendererData.Stats.DrawCalls;
		s_RendererData.Stats.Vertices += verticesCount;
		s_RendererData.Stats.Indeces += indecesCount;
	}

	void Renderer::DrawMesh(const StaticMeshComponent& smComponent, int entityID)
	{
		const Ref<Eagle::StaticMesh>& staticMesh = smComponent.StaticMesh;
		if (staticMesh)
		{
			uint32_t verticesCount = staticMesh->GetVerticesCount();
			uint32_t indecesCount = staticMesh->GetIndecesCount();

			if (verticesCount == 0 || indecesCount == 0)
				return;

			const Ref<Shader>& shader = smComponent.StaticMesh->Material->Shader;
			s_BatchData.Meshes[shader].push_back({ smComponent.StaticMesh, smComponent.GetWorldTransform(), entityID });
		}
	}

	void Renderer::DrawMesh(const Ref<StaticMesh>& staticMesh, const Transform& worldTransform, int entityID)
	{
		if (staticMesh)
		{
			uint32_t verticesCount = staticMesh->GetVerticesCount();
			uint32_t indecesCount = staticMesh->GetIndecesCount();

			if (verticesCount == 0 || indecesCount == 0)
				return;

			const Ref<Shader>& shader = staticMesh->Material->Shader;
			s_BatchData.Meshes[shader].push_back({ staticMesh, worldTransform, entityID });
		}
	}

	void Renderer::DrawSprite(const SpriteComponent& sprite, int entityID)
	{
		s_RendererData.Sprites.push_back( {&sprite, entityID} );
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		s_RendererData.ViewportWidth = width;
		s_RendererData.ViewportHeight = height;
		s_RendererData.GFramebuffer->Resize(width, height);
		s_RendererData.FinalFramebuffer->Resize(width, height);
		s_RendererData.DirectionalShadowFramebuffer->Resize(width * s_RendererData.DirectionalShadowMapResolutionMultiplier, height * s_RendererData.DirectionalShadowMapResolutionMultiplier);
		RenderCommand::SetViewport(0, 0, width, height);

		const float aspectRatio = (float)width / (float)height;
		float orthographicSize = 75.f;
		float orthoLeft = -orthographicSize * aspectRatio * 0.5f;
		float orthoRight = orthographicSize * aspectRatio * 0.5f;
		float orthoBottom = -orthographicSize * 0.5f;
		float orthoTop = orthographicSize * 0.5f;

		s_RendererData.OrthoProjection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, -500.f, 500.f);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		RenderCommand::SetClearColor(color);
	}

	void Renderer::Clear()
	{
		RenderCommand::Clear();
	}

	float& Renderer::Gamma()
	{
		return s_RendererData.Gamma;
	}

	float& Renderer::Exposure()
	{
		return s_RendererData.Exposure;
	}

	Ref<Framebuffer>& Renderer::GetFinalFramebuffer()
	{
		return s_RendererData.FinalFramebuffer;
	}

	Ref<Framebuffer>& Renderer::GetGFramebuffer()
	{
		return s_RendererData.GFramebuffer;
	}

	void Renderer::ResetStats()
	{
		memset(&s_RendererData.Stats, 0, sizeof(Renderer::Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return s_RendererData.Stats;
	}
}