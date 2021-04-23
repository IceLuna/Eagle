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

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000;
		Texture2D::WhiteTexture = Texture2D::Create(1, 1, &whitePixel);
		Texture2D::BlackTexture = Texture2D::Create(1, 1, &blackPixel);

		Renderer2D::Init();
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
}