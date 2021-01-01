#include "egpch.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Eagle
{

	Ref<RendererAPI> Renderer::s_RendererAPI = MakeRef<OpenGLRendererAPI>();
	Ref<Renderer::SceneData> Renderer::m_SceneData = MakeRef<Renderer::SceneData>();
	
	void Renderer::BeginScene(const Camera& camera)
	{
		m_SceneData->ViewProjection = camera.GetViewProjectionMatrix();
	}
	
	void Renderer::EndScene()
	{
	}
	
	void Renderer::Submit(const Ref<Shader> shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();

		std::dynamic_pointer_cast<OpenGLShader>(shader)->SetUniformMat4("u_ViewProjection", m_SceneData->ViewProjection);
		std::dynamic_pointer_cast<OpenGLShader>(shader)->SetUniformMat4("u_Transform", transform);

		vertexArray->Bind();
		s_RendererAPI->DrawIndexed(vertexArray);
	}
}