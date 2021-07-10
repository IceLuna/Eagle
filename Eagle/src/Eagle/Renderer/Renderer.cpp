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
		Ref<Shader> MeshShader;
		Ref<Shader> MeshNormalsShader;
		Ref<Shader> DirectionalShadowMapShader;
		Ref<Shader> PointShadowMapShader;
		Ref<Cubemap> Skybox;
		Ref<UniformBuffer> MatricesUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;
		Ref<Framebuffer> MainFramebuffer;
		Ref<Framebuffer> DirectionalShadowFramebuffer;
		std::array<Ref<Framebuffer>, MAXPOINTLIGHTS> PointShadowFramebuffers;
		glm::mat4 DirectionalLightsView;
		glm::mat4 OrthoProjection;
		glm::mat4 PointLightPerspectiveProjection;
		
		std::vector<SpriteData> Sprites;

		Renderer::Statistics Stats;

		glm::vec3 ViewPos;

		uint32_t CurrentPointLightsSize = 0;

		static constexpr uint32_t SkyboxTextureIndex = 0;
		static constexpr uint32_t DirectionalShadowTextureIndex = 1;
		static constexpr uint32_t PointShadowTextureIndex = 2; //3, 4, 5
		static constexpr uint32_t StartTextureIndex = 6;

		static constexpr uint32_t MatricesUniformBufferSize = sizeof(glm::mat4) * 2;
		static constexpr uint32_t PLStructSize = 448, DLStructSize = 128, SLStructSize = 96, Additional = 8;
		static constexpr uint32_t LightsUniformBufferSize = PLStructSize * MAXPOINTLIGHTS + SLStructSize * MAXPOINTLIGHTS + DLStructSize + Additional;
		static constexpr uint32_t DirectionalShadowMapResolutionMultiplier = 4;
		static constexpr uint32_t LightShadowMapSize = 2048;
		float Gamma = 2.2f;
		uint32_t ViewportWidth = 1, ViewportHeight = 1;

		bool bRenderNormals = false;
	};
	static RendererData s_RendererData;

	struct SMData
	{
		const StaticMeshComponent* smComponent;
		int entityID;
	};

	struct BatchData
	{
		static constexpr uint32_t MaxDrawsPerBatch = 8;
		std::array<int, MaxDrawsPerBatch> EntityIDs;
		std::array<glm::mat4, MaxDrawsPerBatch> Models;
		std::array<float, MaxDrawsPerBatch> TilingFactors;
		std::array<int, MaxDrawsPerBatch> DiffuseTextures;
		std::array<int, MaxDrawsPerBatch> SpecularTextures;
		std::array<int, MaxDrawsPerBatch> NormalTextures;
		static constexpr uint32_t TextureSlotsAvailable = 31 - s_RendererData.StartTextureIndex - 3; // 3 - Number of textures in material
		std::array<float, MaxDrawsPerBatch> Shininess; //Material

		std::vector<MyVertex> Vertices;
		std::vector<uint32_t> Indeces;

		std::map<Ref<Shader>, std::vector<SMData>> Meshes;
		std::unordered_map<Ref<Texture>, int> BoundTextures;
		Ref<UniformBuffer> BatchUniformBuffer;

		uint32_t CurrentVerticesSize = 0;
		uint32_t CurrentIndecesSize = 0;
		uint32_t AlreadyBatchedVerticesSize = 0;
		uint32_t AlreadyBatchedIndecesSize = 0;
		int CurrentlyDrawingIndex = 0;
		const uint32_t BatchUniformBufferSize = 640;
	};
	static BatchData s_BatchData;

	struct {
		bool operator()(const SMData& a, const SMData& b) const 
		{ return glm::length(s_RendererData.ViewPos - a.smComponent->GetWorldTransform().Translation) 
				 < 
				 glm::length(s_RendererData.ViewPos - b.smComponent->GetWorldTransform().Translation); }
	} customMeshesLess;

	struct {
		bool operator()(const SpriteData& a, const SpriteData& b) const {
			return glm::length(s_RendererData.ViewPos - a.Sprite->GetWorldTransform().Translation)
				<
				glm::length(s_RendererData.ViewPos - b.Sprite->GetWorldTransform().Translation);
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

		s_RendererData.MatricesUniformBuffer = UniformBuffer::Create(s_RendererData.MatricesUniformBufferSize, 0);
		s_RendererData.LightsUniformBuffer = UniformBuffer::Create(s_RendererData.LightsUniformBufferSize, 1);

		//Renderer3D Init
		FramebufferSpecification mainFbSpecs;
		FramebufferSpecification directionalShadowFbSpecs;
		FramebufferSpecification pointShadowFbSpecs;
		mainFbSpecs.Width = directionalShadowFbSpecs.Width = 1; //1 for now. After window will be launched, it'll updated viewport's size and so framebuffer will be resized.
		mainFbSpecs.Height = directionalShadowFbSpecs.Height = 1; //1 for now. After window will be launched, it'll updated viewport's size and so framebuffer will be resized.
		mainFbSpecs.Attachments = { {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RGBA8}, {FramebufferTextureFormat::RED_INTEGER}, {FramebufferTextureFormat::DEPTH24STENCIL8} };
		directionalShadowFbSpecs.Attachments = { {FramebufferTextureFormat::DEPTH32F} };

		pointShadowFbSpecs.Width = s_RendererData.LightShadowMapSize;
		pointShadowFbSpecs.Height = s_RendererData.LightShadowMapSize;
		pointShadowFbSpecs.Attachments = { {FramebufferTextureFormat::DEPTH32F, true} };

		s_RendererData.MainFramebuffer = Framebuffer::Create(mainFbSpecs);
		s_RendererData.DirectionalShadowFramebuffer = Framebuffer::Create(directionalShadowFbSpecs);
		
		for (auto& framebuffer : s_RendererData.PointShadowFramebuffers)
			framebuffer = Framebuffer::Create(pointShadowFbSpecs);

		s_RendererData.MeshShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshShader.glsl");
		s_RendererData.MeshShader->BindOnReload(&Renderer::InitMeshShader);
		InitMeshShader();
		s_RendererData.DirectionalShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshDirectionalShadowMapShader.glsl");
		s_RendererData.PointShadowMapShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshPointShadowMapShader.glsl");
		s_RendererData.MeshNormalsShader = ShaderLibrary::GetOrLoad("assets/shaders/MeshNormalsShader.glsl");

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

		s_RendererData.MeshShader->SetIntArray("u_Textures", samplers.data(), samplers.size());
	}

	void Renderer::PrepareRendering()
	{
		s_RendererData.MainFramebuffer->Bind();
		Renderer::Clear();
		s_RendererData.MainFramebuffer->ClearColorAttachment(2, -1); //2 - RED_INTEGER

		Renderer::ResetStats();
		Renderer2D::ResetStats();
	}

	void Renderer::FinishRendering()
	{
		s_RendererData.MainFramebuffer->Unbind();
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
		s_RendererData.ViewPos = cameraComponent.GetWorldTransform().Translation;
		const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Translation;
		s_RendererData.DirectionalLightsView = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardDirection(), glm::vec3(0.f, 1.f, 0.f));
		s_RendererData.DirectionalLightsView = s_RendererData.OrthoProjection * s_RendererData.DirectionalLightsView;

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::PrepareRendering();
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();

		s_RendererData.CurrentPointLightsSize = (uint32_t)pointLights.size();
		s_RendererData.ViewPos = editorCamera.GetTranslation();
		const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Translation;
		s_RendererData.DirectionalLightsView = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardDirection(), glm::vec3(0.f, 1.f, 0.f));
		s_RendererData.DirectionalLightsView = s_RendererData.OrthoProjection * s_RendererData.DirectionalLightsView;
		
		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
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
		memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &s_RendererData.DirectionalLightsView[0][0], sizeof(glm::mat4));
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
		for (uint32_t i = 0; i < spotLightsSize; ++i)
		{
			const glm::vec3& spotLightTranslation = spotLights[i]->GetWorldTransform().Translation;
			const glm::vec3 spotLightForwardVector = spotLights[i]->GetForwardDirection();

			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &spotLightTranslation[0], sizeof(glm::vec3));
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
			ubOffset += 16;
		}
		ubOffset += (MAXSPOTLIGHTS - spotLightsSize) * s_RendererData.SLStructSize; //If not all spot lights used, add additional offset

		//Params of Depth Cubemap
		glm::mat4 pointLightVPs[6];
		constexpr glm::vec3 directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
									glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),  glm::vec3(0.0, 0.0,-1.0) };
		constexpr glm::vec3 upVectors[6] = { glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),
											 glm::vec3(0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0) };
		//PointLight params
		for (uint32_t i = 0; i < pointLightsSize; ++i)
		{
			const glm::vec3& pointLightsTranslation = pointLights[i]->GetWorldTransform().Translation;
			
			for (int j = 0; j < 6; ++j)
				pointLightVPs[j] = s_RendererData.PointLightPerspectiveProjection * glm::lookAt(pointLightsTranslation, pointLightsTranslation + directions[j], upVectors[j]);

			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLightVPs[0][0], sizeof(glm::mat4) * 6);
			ubOffset += sizeof(glm::mat4) * 6;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLightsTranslation[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Ambient[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->LightColor[0], sizeof(glm::vec3));
			ubOffset += 16;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Specular[0], sizeof(glm::vec3));
			ubOffset += 12;
			memcpy_s(uniformBuffer + ubOffset, uniformBufferSize, &pointLights[i]->Distance, sizeof(float));
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

	void Renderer::DrawSkybox(const Ref<Cubemap>& cubemap)
	{
		s_RendererData.Skybox = cubemap;
		s_RendererData.Skybox->Bind(s_RendererData.SkyboxTextureIndex);
	}

	void Renderer::EndScene()
	{
		std::sort(std::begin(s_RendererData.Sprites), std::end(s_RendererData.Sprites), customSpritesLess);
		for (auto& mesh : s_BatchData.Meshes)
		{
			auto& meshes = mesh.second;
			std::sort(std::begin(meshes), std::end(meshes), customMeshesLess);
		}

		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();
		RenderInfo mainRenderInfo = { DrawToShadowMap::None, -1, true };
		RenderInfo pointLightRenderInfo = { DrawToShadowMap::Point, -1, true };
		RenderInfo directionalLightRenderInfo = { DrawToShadowMap::Directional, -1, false };
		//Rendering to ShadowMap
		RenderCommand::SetViewport(0, 0, s_RendererData.ViewportWidth * s_RendererData.DirectionalShadowMapResolutionMultiplier, s_RendererData.ViewportHeight * s_RendererData.DirectionalShadowMapResolutionMultiplier);
		s_RendererData.DirectionalShadowFramebuffer->Bind();
		RenderCommand::ClearDepthBuffer();
		DrawPassedMeshes(directionalLightRenderInfo);
		DrawPassedSprites(s_RendererData.ViewPos, directionalLightRenderInfo);

		RenderCommand::SetViewport(0, 0, s_RendererData.LightShadowMapSize, s_RendererData.LightShadowMapSize);
		for (int i = 0; i < s_RendererData.CurrentPointLightsSize; ++i)
		{
			pointLightRenderInfo.pointLightIndex = i;
			s_RendererData.PointShadowFramebuffers[i]->Bind();
			RenderCommand::ClearDepthBuffer();
			//Rendering to ShadowMap
			DrawPassedMeshes(pointLightRenderInfo);
			DrawPassedSprites(s_RendererData.ViewPos, pointLightRenderInfo);
		}

		//Rendering to Color Attachments
		RenderCommand::SetViewport(0, 0, s_RendererData.ViewportWidth, s_RendererData.ViewportHeight);
		s_RendererData.MainFramebuffer->Bind();
		s_RendererData.DirectionalShadowFramebuffer->BindDepthTexture(s_RendererData.DirectionalShadowTextureIndex, 0);

		for (int i = 0; i < s_RendererData.CurrentPointLightsSize; ++i)
			s_RendererData.PointShadowFramebuffers[i]->BindDepthTexture(s_RendererData.PointShadowTextureIndex + i, 0);

		Renderer::ResetStats();
		Renderer2D::ResetStats();
		DrawPassedMeshes(mainRenderInfo);
		DrawPassedSprites(s_RendererData.ViewPos, mainRenderInfo);

		Renderer::FinishRendering();
		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		s_RendererData.Stats.RenderingTook = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;
	}

	void Renderer::DrawPassedMeshes(const RenderInfo& renderInfo)
	{
		s_BatchData.AlreadyBatchedVerticesSize = s_BatchData.AlreadyBatchedIndecesSize = 0;
		s_BatchData.CurrentVerticesSize = s_BatchData.CurrentIndecesSize = 0;

		const Ref<Shader>* shadowShader = nullptr;
		bool bDrawToShadowMap = renderInfo.drawToShadowMap != DrawToShadowMap::None;
		if (renderInfo.drawToShadowMap == DrawToShadowMap::Directional)
			shadowShader = &s_RendererData.DirectionalShadowMapShader;
		else if (renderInfo.drawToShadowMap == DrawToShadowMap::Point)
		{
			shadowShader = &s_RendererData.PointShadowMapShader;
			(*shadowShader)->Bind();
			(*shadowShader)->SetInt("u_PointLightIndex", renderInfo.pointLightIndex);
		}

		if (!renderInfo.bRedraw)
		{
			s_BatchData.Vertices.clear();
			s_BatchData.Indeces.clear();
		}

		for (auto it : s_BatchData.Meshes)
		{
			const Ref<Shader>& shader = it.first;
			const std::vector<SMData>& smData = it.second;
			uint32_t textureIndex = s_RendererData.StartTextureIndex;
			StartBatch();

			for (auto& sm : smData)
			{
				const StaticMeshComponent* smComponent = sm.smComponent;
				const int entityID = sm.entityID;

				auto& diffuseTexture = smComponent->StaticMesh->Material->DiffuseTexture;
				auto& specularTexture = smComponent->StaticMesh->Material->SpecularTexture;
				auto& normalTexture = smComponent->StaticMesh->Material->NormalTexture;
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
						FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, bDrawToShadowMap, renderInfo.bRedraw);
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

				const std::vector<Vertex>& vertices = smComponent->StaticMesh->GetVertices();
				const std::vector<uint32_t>& indeces = smComponent->StaticMesh->GetIndeces();
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

				const Transform& transform = smComponent->GetWorldTransform();
				glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

				s_BatchData.EntityIDs[s_BatchData.CurrentlyDrawingIndex] = entityID;
				s_BatchData.Models[s_BatchData.CurrentlyDrawingIndex] = transformMatrix;
				s_BatchData.DiffuseTextures[s_BatchData.CurrentlyDrawingIndex] = diffuseTextureSlot;
				s_BatchData.SpecularTextures[s_BatchData.CurrentlyDrawingIndex] = specularTextureSlot;
				s_BatchData.NormalTextures[s_BatchData.CurrentlyDrawingIndex] = normalTextureSlot;
				s_BatchData.Shininess[s_BatchData.CurrentlyDrawingIndex] = smComponent->StaticMesh->Material->Shininess;
				s_BatchData.TilingFactors[s_BatchData.CurrentlyDrawingIndex] = smComponent->StaticMesh->Material->TilingFactor;

				++s_BatchData.CurrentlyDrawingIndex;

				if (s_BatchData.CurrentlyDrawingIndex == s_BatchData.MaxDrawsPerBatch)
				{
					FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, bDrawToShadowMap, renderInfo.bRedraw);
					StartBatch();
				}
			}

			if (s_BatchData.CurrentlyDrawingIndex)
				FlushMeshes(bDrawToShadowMap ? *shadowShader : shader, bDrawToShadowMap, renderInfo.bRedraw);
		}
	}

	void Renderer::DrawPassedSprites(const glm::vec3& cameraPosition, const RenderInfo& renderInfo)
	{
		bool bDrawToShadowMap = renderInfo.drawToShadowMap != DrawToShadowMap::None;
		Renderer2D::BeginScene(cameraPosition, renderInfo);
		if (!bDrawToShadowMap && s_RendererData.Skybox)
			Renderer2D::DrawSkybox(s_RendererData.Skybox);
		if (!Renderer2D::IsRedrawing())
		{
			for (auto& spriteData : s_RendererData.Sprites)
			{
				const SpriteComponent* sprite = spriteData.Sprite;
				const int entityID = spriteData.EntityID;
				const auto& material = sprite->Material;

				if (sprite->bSubTexture)
					Renderer2D::DrawQuad(sprite->GetWorldTransform(), sprite->SubTexture, { 1.f, material->TilingFactor, material->Shininess }, (int)entityID);
				else
					Renderer2D::DrawQuad(sprite->GetWorldTransform(), material, (int)entityID);
			}
		}
		Renderer2D::EndScene();
	}

	void Renderer::FlushMeshes(const Ref<Shader>& shader, bool bDrawToShadowMap, bool bRedrawing)
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
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.EntityIDs[i], sizeof(int));
			offset += 4;
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.TilingFactors[i], sizeof(float));
			offset += 4;
			memcpy_s(buffer + offset, bufferSize, &s_BatchData.Shininess[i], sizeof(float));
			offset += 8;
		}

		offset += (s_BatchData.MaxDrawsPerBatch - s_BatchData.CurrentlyDrawingIndex) * 16;

		s_BatchData.BatchUniformBuffer->Bind();
		s_BatchData.BatchUniformBuffer->UpdateData(buffer, bufferSize, 0);
		delete[] buffer;

		shader->Bind();
		if (!bDrawToShadowMap)
		{
			int samplers[4];
			for (int i = 0; i < 4; ++i)
				samplers[i] = s_RendererData.PointShadowTextureIndex + i;
			shader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);
			shader->SetIntArray("u_PointShadowCubemaps", samplers, MAXPOINTLIGHTS);
			shader->SetInt("u_ShadowMap", s_RendererData.DirectionalShadowTextureIndex);
			shader->SetFloat3("u_ViewPos", s_RendererData.ViewPos);
			bool bSkybox = s_RendererData.Skybox.operator bool();
			shader->SetInt("u_SkyboxEnabled", int(bSkybox));
			shader->SetFloat("u_Gamma", s_RendererData.Gamma);

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

		if (!bDrawToShadowMap && s_RendererData.bRenderNormals)
		{
			s_RendererData.MeshNormalsShader->Bind();
			RenderCommand::DrawIndexed(indecesCount);
		}

		++s_RendererData.Stats.DrawCalls;
		s_RendererData.Stats.Vertices += verticesCount;
		s_RendererData.Stats.Indeces += indecesCount;
	}

	void Renderer::DrawMesh(const StaticMeshComponent& smComponent, int entityID)
	{
		const Ref<Eagle::StaticMesh>& staticMesh = smComponent.StaticMesh;

		uint32_t verticesCount = staticMesh->GetVerticesCount();
		uint32_t indecesCount = staticMesh->GetIndecesCount();

		if (verticesCount == 0 || indecesCount == 0)
			return;

		const Ref<Shader>& shader = smComponent.StaticMesh->Material->Shader;
		s_BatchData.Meshes[shader].push_back({ &smComponent, entityID });
	}

	void Renderer::DrawSprite(const SpriteComponent& sprite, int entityID)
	{
		s_RendererData.Sprites.push_back( {&sprite, entityID} );
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		s_RendererData.MainFramebuffer->Resize(width, height);
		s_RendererData.DirectionalShadowFramebuffer->Resize(width * s_RendererData.DirectionalShadowMapResolutionMultiplier, height * s_RendererData.DirectionalShadowMapResolutionMultiplier);
		RenderCommand::SetViewport(0, 0, width, height);
		s_RendererData.ViewportWidth = width;
		s_RendererData.ViewportHeight = height;

		const float aspectRatio = (float)width / (float)height;
		float orthographicSize = 75.f;
		float orthoLeft = -orthographicSize * aspectRatio * 0.5f;
		float orthoRight = orthographicSize * aspectRatio * 0.5f;
		float orthoBottom = -orthographicSize * 0.5f;
		float orthoTop = orthographicSize * 0.5f;

		constexpr float pointLightAspectRatio = (float)s_RendererData.LightShadowMapSize / (float)s_RendererData.LightShadowMapSize;
		s_RendererData.OrthoProjection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, -500.f, 500.f);
		s_RendererData.PointLightPerspectiveProjection = glm::perspective(glm::radians(90.f), pointLightAspectRatio, 0.01f, 10000.f);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		RenderCommand::SetClearColor(color);
	}

	void Renderer::SetRenderNormals(bool bRender)
	{
		s_RendererData.bRenderNormals = bRender;
	}

	bool Renderer::IsRenderingNormals()
	{
		return s_RendererData.bRenderNormals;
	}

	void Renderer::Clear()
	{
		RenderCommand::Clear();
	}

	float& Renderer::Gamma()
	{
		return s_RendererData.Gamma;
	}

	Ref<Framebuffer>& Renderer::GetMainFramebuffer()
	{
		return s_RendererData.MainFramebuffer;
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