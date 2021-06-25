#include "egpch.h"

#include "Renderer.h"
#include "Renderer2D.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
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

		const uint32_t SkyboxTextureIndex = 0;
		const uint32_t DiffuseTextureIndex = 1;
		const uint32_t SpecularTextureIndex = 2;

		const uint32_t MatricesUniformBufferSize = sizeof(glm::mat4) * 2;
		const uint32_t PLStructSize = 64, DLStructSize = 64, SLStructSize = 96, Additional = 8;
		const uint32_t LightsUniformBufferSize = PLStructSize * MAXPOINTLIGHTS + SLStructSize * MAXPOINTLIGHTS + DLStructSize + Additional;

		bool bRenderNormals = false;
	};

	static RendererData s_RendererData;

	void Renderer::Init()
	{
		RenderCommand::Init();

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000;
		Texture2D::WhiteTexture = Texture2D::Create(1, 1, &whitePixel);
		Texture2D::WhiteTexture->m_Path = "White";
		Texture2D::BlackTexture = Texture2D::Create(1, 1, &blackPixel);
		Texture2D::BlackTexture->m_Path = "Black";
		Texture2D::NoneTexture = Texture2D::Create("assets/textures/Editor/none.png", false);
		Texture2D::MeshIconTexture = Texture2D::Create("assets/textures/Editor/meshicon.png", false);
		Texture2D::TextureIconTexture = Texture2D::Create("assets/textures/Editor/textureicon.png", false);
		Texture2D::SceneIconTexture = Texture2D::Create("assets/textures/Editor/sceneicon.png", false);
		Texture2D::FolderIconTexture = Texture2D::Create("assets/textures/Editor/foldericon.png", false);
		Texture2D::UnknownIconTexture = Texture2D::Create("assets/textures/Editor/unknownicon.png", false);

		s_RendererData.MatricesUniformBuffer = UniformBuffer::Create(s_RendererData.MatricesUniformBufferSize, 0);
		s_RendererData.LightsUniformBuffer = UniformBuffer::Create(s_RendererData.LightsUniformBufferSize, 1);

		//Renderer3D Init
		s_RendererData.MeshShader = Shader::Create("assets/shaders/StaticMeshShader.glsl");
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);

		s_RendererData.MeshNormalsShader = Shader::Create("assets/shaders/RenderMeshNormalsShader.glsl");

		BufferLayout bufferLayout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float3, "a_Normal"},
			{ShaderDataType::Float2, "a_TexCoord"}
		};

		s_RendererData.ib = IndexBuffer::Create();
		s_RendererData.vb = VertexBuffer::Create();
		s_RendererData.vb->SetLayout(bufferLayout);

		s_RendererData.va = VertexArray::Create();
		s_RendererData.va->AddVertexBuffer(s_RendererData.vb);
		s_RendererData.va->SetIndexBuffer(s_RendererData.ib);
		s_RendererData.va->Unbind();
		
		//Renderer2D Init
		Renderer2D::Init();
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetFloat3("u_ViewPos", cameraComponent.GetWorldTransform().Translation);
		s_RendererData.MeshShader->SetInt("u_SkyboxEnabled", 0);

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);

		//StartBatch();
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();
		const glm::vec3 cameraPos = editorCamera.GetTranslation();
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetFloat3("u_ViewPos", cameraPos);
		s_RendererData.MeshShader->SetInt("u_SkyboxEnabled", 0);

		SetupMatricesUniforms(cameraView, cameraProjection);
		SetupLightUniforms(pointLights, directionalLight, spotLights);
		
		//StartBatch();
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
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetInt("u_SkyboxEnabled", 1);
	}

	void Renderer::EndScene()
	{
		s_RendererData.Skybox.reset();
	}

	void Renderer::Draw(const StaticMeshComponent& smComponent, int entityID)
	{
		const Ref<Eagle::StaticMesh>& staticMesh = smComponent.StaticMesh;

		uint32_t verticesCount = staticMesh->GetVerticesCount();
		uint32_t indecesCount = staticMesh->GetIndecesCount();

		if (verticesCount == 0 || indecesCount == 0)
			return;

		const Transform& transform = smComponent.GetWorldTransform();
		glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), transform.Translation);
		transformMatrix *= Math::GetRotationMatrix(transform.Rotation);
		transformMatrix = glm::scale(transformMatrix, { transform.Scale3D.x, transform.Scale3D.y, transform.Scale3D.z });
		
		const Material& material = staticMesh->Material;

		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetMat4("u_Model", transformMatrix);
		s_RendererData.MeshShader->SetInt("u_EntityID", entityID);
		s_RendererData.MeshShader->SetInt("u_DiffuseTexture", s_RendererData.DiffuseTextureIndex);
		s_RendererData.MeshShader->SetInt("u_SpecularTexture", s_RendererData.SpecularTextureIndex);
		s_RendererData.MeshShader->SetFloat("u_Material.Shininess", material.Shininess);
		s_RendererData.MeshShader->SetFloat("u_TilingFactor", material.TilingFactor);
		
		material.DiffuseTexture->Bind(s_RendererData.DiffuseTextureIndex);
		material.SpecularTexture->Bind(s_RendererData.SpecularTextureIndex);
		
		s_RendererData.va->Bind();
		
		s_RendererData.ib->Bind();
		s_RendererData.ib->SetData(staticMesh->GetIndecesData(), indecesCount);
		s_RendererData.vb->Bind();
		s_RendererData.vb->SetData(staticMesh->GetVerticesData(), sizeof(Vertex) * verticesCount);

			++s_RendererData.Stats.DrawCalls;
		s_RendererData.Stats.Vertices += verticesCount;
		s_RendererData.Stats.Indeces += indecesCount;

		RenderCommand::DrawIndexed(indecesCount);

		if (s_RendererData.bRenderNormals)
		{
			s_RendererData.MeshNormalsShader->Bind();
			s_RendererData.MeshNormalsShader->SetMat4("u_Model", transformMatrix);
			RenderCommand::DrawIndexed(indecesCount);
		}
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

	void Renderer::ResetStats()
	{
		memset(&s_RendererData.Stats, 0, sizeof(Renderer::Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return s_RendererData.Stats;
	}
}