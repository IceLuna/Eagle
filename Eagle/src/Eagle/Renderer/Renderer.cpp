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
		Ref<Cubemap> Skybox;

		Renderer::Statistics Stats;

		const uint32_t SkyboxTextureIndex = 0;
		const uint32_t DiffuseTextureIndex = 1;
		const uint32_t SpecularTextureIndex = 2;
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


		//Renderer3D Init
		s_RendererData.MeshShader = Shader::Create("assets/shaders/StaticMeshShader.glsl");
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetInt("u_Skybox", s_RendererData.SkyboxTextureIndex);

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
		const glm::mat4 cameraVP = cameraComponent.GetViewProjection();
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetMat4("u_ViewProjection", cameraVP);
		s_RendererData.MeshShader->SetFloat3("u_ViewPos", cameraComponent.GetWorldTransform().Translation);
		s_RendererData.MeshShader->SetInt("u_SkyboxEnabled", 0);

		char uniformTextBuffer[64];
		//PointLight params
		for (int i = 0; i < pointLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Position", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Ambient", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->Ambient);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Diffuse", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Specular", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Distance", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, pointLights[i]->Distance);
		}
		s_RendererData.MeshShader->SetInt("u_PointLightsSize", (int)pointLights.size());

		//DirectionalLight params
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Direction", directionalLight.GetForwardDirection());
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Ambient", directionalLight.Ambient);
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Diffuse", directionalLight.LightColor);
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Specular", directionalLight.Specular);

		//SpotLight params
		for (int i = 0; i < spotLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Position", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Direction", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetForwardDirection());
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Diffuse", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Specular", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].InnerCutOffAngle", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, spotLights[i]->InnerCutOffAngle);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].OuterCutOffAngle", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, spotLights[i]->OuterCutOffAngle);
		}
		s_RendererData.MeshShader->SetInt("u_SpotLightsSize", (int)spotLights.size());

		//StartBatch();
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraVP = editorCamera.GetViewProjection();
		const glm::vec3 cameraPos = editorCamera.GetTranslation();
		s_RendererData.MeshShader->Bind();
		s_RendererData.MeshShader->SetMat4("u_ViewProjection", cameraVP);
		s_RendererData.MeshShader->SetFloat3("u_ViewPos", cameraPos);
		s_RendererData.MeshShader->SetInt("u_SkyboxEnabled", 0);

		//PointLight params
		char uniformTextBuffer[64];
		for (int i = 0; i < pointLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Position", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Ambient", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->Ambient);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Diffuse", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Specular", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, pointLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_PointLights[%d].Distance", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, pointLights[i]->Distance);
		}
		s_RendererData.MeshShader->SetInt("u_PointLightsSize", (int)pointLights.size());

		//DirectionalLight params
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Direction", directionalLight.GetForwardDirection());
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Ambient", directionalLight.Ambient);
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Diffuse", directionalLight.LightColor);
		s_RendererData.MeshShader->SetFloat3("u_DirectionalLight.Specular", directionalLight.Specular);

		//SpotLight params
		for (int i = 0; i < spotLights.size(); ++i)
		{
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Position", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetWorldTransform().Translation);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Direction", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->GetForwardDirection());
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Diffuse", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->LightColor);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].Specular", i);
			s_RendererData.MeshShader->SetFloat3(uniformTextBuffer, spotLights[i]->Specular);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].InnerCutOffAngle", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, spotLights[i]->InnerCutOffAngle);
			sprintf_s(uniformTextBuffer, 64, "u_SpotLights[%d].OuterCutOffAngle", i);
			s_RendererData.MeshShader->SetFloat(uniformTextBuffer, spotLights[i]->OuterCutOffAngle);
		}
		s_RendererData.MeshShader->SetInt("u_SpotLightsSize", (int)spotLights.size());

		//StartBatch();
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
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		RenderCommand::SetClearColor(color);
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