#include "egpch.h"

#include "Renderer.h"
#include "Renderer2D.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	
	Ref<Renderer::SceneData> Renderer::s_SceneData = MakeRef<Renderer::SceneData>();
	
	void Renderer::BeginScene(const CameraComponent& cameraComponent)
	{
		s_SceneData->ViewProjection = cameraComponent.GetViewProjection();
	}
	
	void Renderer::EndScene()
	{
	}

	void Renderer::Init()
	{
		RenderCommand::Init();
		Renderer2D::Init();
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}
	
	void Renderer::Submit(const Ref<Shader> shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();

		shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjection);
		shader->SetMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

	void Renderer::SetClearColor(const glm::vec4& color)
	{
		RenderCommand::SetClearColor(color);
	}
	
	void Renderer::Clear()
	{
		RenderCommand::Clear();
	}
}