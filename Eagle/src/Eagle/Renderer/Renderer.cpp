#include "egpch.h"

#include "Renderer.h"
#include "Renderer2D.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct MyVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		int Index;

		MyVertex() = default;
		MyVertex(const MyVertex&) = default;

		MyVertex(const Vertex& vertex)
		: Position(vertex.Position)
		, Normal(vertex.Normal)
		, TexCoords(vertex.TexCoords)
		, Index(0)
		{}

		MyVertex& operator=(const MyVertex& vertex) = default;

		MyVertex& operator=(const Vertex& vertex)
		{
			Position = vertex.Position;
			Normal = vertex.Normal;
			TexCoords = vertex.TexCoords;
			return *this;
		}
	};

	struct RendererData
	{
		Ref<VertexArray> va;
		Ref<IndexBuffer> ib;
		Ref<VertexBuffer> vb;
		Ref<Shader> MeshShader;
		Ref<Shader> MeshNormalsShader;
		Ref<Cubemap> Skybox;
		Ref<UniformBuffer> MatricesUniformBuffer;
		Ref<UniformBuffer> LightsUniformBuffer;

		Renderer::Statistics Stats;

		glm::vec3 ViewPos;

		const uint32_t SkyboxTextureIndex = 0;
		const uint32_t DiffuseTextureIndex = 1;
		const uint32_t SpecularTextureIndex = 2;

		const uint32_t MatricesUniformBufferSize = sizeof(glm::mat4) * 2;
		const uint32_t PLStructSize = 64, DLStructSize = 64, SLStructSize = 96, Additional = 8;
		const uint32_t LightsUniformBufferSize = PLStructSize * MAXPOINTLIGHTS + SLStructSize * MAXPOINTLIGHTS + DLStructSize + Additional;
		float Gamma = 2.2f;

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
		static constexpr uint32_t MaxDrawsPerBatch = 15;
		std::array<int, MaxDrawsPerBatch> EntityIDs;
		std::array<glm::mat4, MaxDrawsPerBatch> Models;
		std::array<float, MaxDrawsPerBatch> TilingFactors;
		std::array<int, MaxDrawsPerBatch> DiffuseTextures;
		std::array<int, MaxDrawsPerBatch> SpecularTextures;
		std::array<float, MaxDrawsPerBatch> Shininess; //Material

		std::vector<MyVertex> Vertices;
		std::vector<uint32_t> Indeces;

		std::map<Ref<Shader>, std::vector<SMData>> Meshes;
		std::unordered_map<Ref<Texture>, uint32_t> BoundTextures;
		Ref<UniformBuffer> BatchUniformBuffer;

		int CurrentlyDrawingIndex = 0;
		const uint32_t BatchUniformBufferSize = 1200;
	};
	static BatchData s_BatchData;

	void Renderer::Init()
	{
		RenderCommand::Init();

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
		s_RendererData.MeshShader = ShaderLibrary::GetOrLoad("assets/shaders/StaticMeshShader.glsl");

		s_RendererData.MeshNormalsShader = ShaderLibrary::GetOrLoad("assets/shaders/RenderMeshNormalsShader.glsl");

		BufferLayout bufferLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float3, "a_Normal"},
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

		s_BatchData.Vertices.reserve(1'000'000);
		s_BatchData.Indeces.reserve(1'000'000);
		s_BatchData.BatchUniformBuffer = UniformBuffer::Create(s_BatchData.BatchUniformBufferSize, 2);
		s_BatchData.DiffuseTextures.fill(1);
		s_BatchData.SpecularTextures.fill(1);
		
		//Renderer2D Init
		Renderer2D::Init();
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();

		s_RendererData.ViewPos = cameraComponent.GetWorldTransform().Translation;

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();
		const glm::vec3 cameraPos = editorCamera.GetTranslation();

		s_RendererData.ViewPos = cameraPos;

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
	}

	void Renderer::StartBatch()
	{
		s_BatchData.Vertices.clear();
		s_BatchData.Indeces.clear();
		s_BatchData.CurrentlyDrawingIndex = 0;
		s_BatchData.BoundTextures.clear();
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

		//PointLight params
		for (uint32_t i = 0; i < pointLightsSize; ++i)
		{
			const glm::vec3& pointLightsTranslation = pointLights[i]->GetWorldTransform().Translation;

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

	void Renderer::ReflectSkybox(const Ref<Cubemap>& cubemap)
	{
		s_RendererData.Skybox = cubemap;
		s_RendererData.Skybox->Bind(s_RendererData.SkyboxTextureIndex);
	}

	void Renderer::EndScene()
	{
		for (auto it : s_BatchData.Meshes)
		{
			const Ref<Shader>& shader = it.first;
			const std::vector<SMData>& smData = it.second;
			uint32_t textureIndex = 1;
			StartBatch();

			for (auto& sm : smData)
			{
				const StaticMeshComponent* smComponent = sm.smComponent;
				const int entityID = sm.entityID;

				auto& diffuseTexture = smComponent->StaticMesh->Material->DiffuseTexture;
				auto& specularTexture = smComponent->StaticMesh->Material->SpecularTexture;
				auto itDiffuse = s_BatchData.BoundTextures.find(diffuseTexture);
				auto itSpecular = s_BatchData.BoundTextures.find(specularTexture);

				if (s_BatchData.BoundTextures.size() > 29)
				{
					if (itDiffuse == s_BatchData.BoundTextures.end()
						|| itSpecular == s_BatchData.BoundTextures.end())
					{
						Flush(shader);
						StartBatch();
						itDiffuse = itSpecular = s_BatchData.BoundTextures.end();
					}
				}

				const std::vector<Vertex>& vertices = smComponent->StaticMesh->GetVertices();
				const std::vector<uint32_t>& indeces = smComponent->StaticMesh->GetIndeces();

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

				const Transform& transform = smComponent->GetWorldTransform();
				glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

				uint32_t diffuseTextureSlot = 0;
				bool bRepeatSearch = false;
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

				uint32_t specularTextureSlot = 0;
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

				s_BatchData.EntityIDs[s_BatchData.CurrentlyDrawingIndex] = entityID;
				s_BatchData.Models[s_BatchData.CurrentlyDrawingIndex] = transformMatrix;
				s_BatchData.DiffuseTextures[s_BatchData.CurrentlyDrawingIndex] = diffuseTextureSlot;
				s_BatchData.SpecularTextures[s_BatchData.CurrentlyDrawingIndex] = specularTextureSlot;
				s_BatchData.Shininess[s_BatchData.CurrentlyDrawingIndex] = smComponent->StaticMesh->Material->Shininess;
				s_BatchData.TilingFactors[s_BatchData.CurrentlyDrawingIndex] = smComponent->StaticMesh->Material->TilingFactor;

				++s_BatchData.CurrentlyDrawingIndex;

				if (s_BatchData.CurrentlyDrawingIndex == s_BatchData.MaxDrawsPerBatch)
				{
					Flush(shader);
					StartBatch();
				}
			}

			if (s_BatchData.CurrentlyDrawingIndex)
				Flush(shader);
		}

		s_RendererData.Skybox.reset();
		s_BatchData.Meshes.clear();
	}

	void Renderer::Flush(const Ref<Shader>& shader)
	{
		if (s_BatchData.CurrentlyDrawingIndex == 0)
			return;

		uint32_t verticesCount = (uint32_t)s_BatchData.Vertices.size();
		uint32_t indecesCount = (uint32_t)s_BatchData.Indeces.size();
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
		shader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);
		shader->SetFloat3("u_ViewPos", s_RendererData.ViewPos);
		bool bSkybox = s_RendererData.Skybox.operator bool();
		shader->SetInt("u_SkyboxEnabled", int(bSkybox));
		shader->SetFloat("gamma", s_RendererData.Gamma);

		shader->SetIntArray("u_DiffuseTextures", s_BatchData.DiffuseTextures.data(), (uint32_t)s_BatchData.DiffuseTextures.size());
		shader->SetIntArray("u_SpecularTextures", s_BatchData.SpecularTextures.data(), (uint32_t)s_BatchData.SpecularTextures.size());

		s_RendererData.va->Bind();
		s_RendererData.ib->Bind();
		s_RendererData.ib->SetData(s_BatchData.Indeces.data(), indecesCount);
		s_RendererData.vb->Bind();
		s_RendererData.vb->SetData(s_BatchData.Vertices.data(), sizeof(MyVertex) * verticesCount);

		RenderCommand::DrawIndexed(indecesCount);

		if (s_RendererData.bRenderNormals)
		{
			s_RendererData.MeshNormalsShader->Bind();
			RenderCommand::DrawIndexed(indecesCount);
		}

		++s_RendererData.Stats.DrawCalls;
		s_RendererData.Stats.Vertices += verticesCount;
		s_RendererData.Stats.Indeces += indecesCount;
	}

	void Renderer::Draw(const StaticMeshComponent& smComponent, int entityID)
	{
		const Ref<Eagle::StaticMesh>& staticMesh = smComponent.StaticMesh;

		uint32_t verticesCount = staticMesh->GetVerticesCount();
		uint32_t indecesCount = staticMesh->GetIndecesCount();

		if (verticesCount == 0 || indecesCount == 0)
			return;

		const Ref<Shader>& shader = smComponent.StaticMesh->Material->Shader;
		s_BatchData.Meshes[shader].push_back({ &smComponent, entityID });
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
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

	void Renderer::ResetStats()
	{
		memset(&s_RendererData.Stats, 0, sizeof(Renderer::Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return s_RendererData.Stats;
	}
}