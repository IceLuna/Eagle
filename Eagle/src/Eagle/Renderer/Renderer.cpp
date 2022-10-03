#include "egpch.h"

#include "Renderer.h"
#include "Renderer2D.h"
#include "Buffer.h"
#include "Framebuffer.h"
#include "PipelineGraphics.h"
#include "PipelineCompute.h"
#include "StagingManager.h"
#include "RenderCommandManager.h"
#include "Fence.h"
#include "Semaphore.h"

#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanSwapchain.h"

//TODO: remove this dependency
#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct SpriteData
	{
		const SpriteComponent* Sprite = nullptr;
		int EntityID = -1;
	};

	struct LineData
	{
		glm::vec3 Start = glm::vec3{0.f};
		glm::vec3 End = glm::vec3{0.f};
		glm::vec4 Color = glm::vec4{0.f, 0.f, 0.f, 1.f};
	};

	struct RendererData
	{
		Ref<DescriptorManager> DescriptorManager;
		Ref<VulkanSwapchain> Swapchain;

		Ref<Shader> PresentVertexShader;
		Ref<Shader> PresentFragmentShader;
		Ref<PipelineGraphics> PresentPipeline;
		std::vector<Ref<Framebuffer>> PresentFramebuffers;
		Ref<Image> FinalImage;

		Ref<ImGuiLayer>* ImGuiLayer; // Pointer is used just to avoid incrementing counter

		Ref<CommandManager> GraphicsCommandManager;
		std::vector<Ref<CommandBuffer>> CommandBuffers;
		std::vector<Ref<Fence>> Fences;
		std::vector<Ref<Semaphore>> Semaphores;

		glm::mat4 OrthoProjection = glm::mat4(1.f);
		glm::vec3 ViewPos;
		
		std::vector<SpriteData> Sprites;
		std::vector<LineData> Lines;

		Renderer::Statistics Stats;

		float Gamma = 2.2f;
		float Exposure = 1.f;
		glm::uvec2 ViewportSize = glm::uvec2(0, 0);
		uint32_t FramesInFlight = 0;
		uint32_t CurrentFrameIndex = 0;
	};

	struct ShaderDependencies
	{
		std::vector<Ref<Pipeline>> Pipelines;
	};

	static RendererData* s_RendererData = nullptr;
	static RenderCommandQueue s_CommandQueue;
	static RenderCommandQueue s_ResourceFreeQueue[3]; //RendererConfig::FramesInFlight

	static Scope<RendererAPI> s_RendererAPI;
	static std::unordered_map<size_t, ShaderDependencies> s_ShaderDependencies;

	struct SMData
	{
		Ref<StaticMesh> StaticMesh;
		Transform WorldTransform;
		int entityID;
	};

	struct {
		bool operator()(const SMData& a, const SMData& b) const 
		{ return glm::length(s_RendererData->ViewPos - a.WorldTransform.Location)
				 < 
				 glm::length(s_RendererData->ViewPos - b.WorldTransform.Location); }
	} customMeshesLess;

	struct {
		bool operator()(const SpriteData& a, const SpriteData& b) const {
			return glm::length(s_RendererData->ViewPos - a.Sprite->GetWorldTransform().Location)
				<
				glm::length(s_RendererData->ViewPos - b.Sprite->GetWorldTransform().Location);
		}
	} customSpritesLess;

	static Scope<VulkanRenderer> CreateRendererAPI()
	{
		switch (RendererAPI::Current())
		{
			case RendererAPIType::Vulkan: return MakeScope<VulkanRenderer>();
		}
		EG_CORE_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	static void SetupPresentPipeline()
	{
		s_RendererData->PresentVertexShader = Shader::Create("assets/shaders/present.vert", ShaderType::Vertex);
		s_RendererData->PresentFragmentShader = Shader::Create("assets/shaders/present.frag", ShaderType::Fragment);

		auto& swapchainImages = s_RendererData->Swapchain->GetImages();

		ColorAttachment colorAttachment;
		colorAttachment.Image = swapchainImages[0];
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageLayoutType::Present;
		colorAttachment.bClearEnabled = true;
		colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

		PipelineGraphicsState state;
		state.VertexShader = s_RendererData->PresentVertexShader;
		state.FragmentShader = s_RendererData->PresentFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::None;

		s_RendererData->PresentPipeline = PipelineGraphics::Create(state);

		for (auto& image : swapchainImages)
			s_RendererData->PresentFramebuffers.push_back(Framebuffer::Create({ image }, s_RendererData->ViewportSize, s_RendererData->PresentPipeline->GetRenderPassHandle()));
	}

	void Renderer::Init()
	{
		s_RendererData = new RendererData();
		s_RendererData->Swapchain = Application::Get().GetWindow().GetSwapchain();
		s_RendererData->ViewportSize = s_RendererData->Swapchain->GetSize();
		s_RendererData->FramesInFlight = Renderer::GetConfig().FramesInFlight;
		s_RendererAPI = CreateRendererAPI();

		s_RendererData->Swapchain->SetOnSwapchainRecreatedCallback([data = s_RendererData]()
		{
			data->PresentFramebuffers.clear();

			auto& swapchainImages = data->Swapchain->GetImages();
			const void* renderPassHandle = data->PresentPipeline->GetRenderPassHandle();
			glm::uvec2 size = data->Swapchain->GetSize();
			data->ViewportSize = size;
			data->FinalImage->Resize({ size, 1 });
			for (auto& image : swapchainImages)
				data->PresentFramebuffers.push_back(Framebuffer::Create({ image }, data->ViewportSize, data->PresentPipeline->GetRenderPassHandle()));
		});

		s_RendererData->GraphicsCommandManager = CommandManager::Create(CommandQueueFamily::Graphics, true);
		s_RendererData->CommandBuffers.reserve(s_RendererData->FramesInFlight);
		s_RendererData->Fences.reserve(s_RendererData->FramesInFlight);
		s_RendererData->Semaphores.reserve(s_RendererData->FramesInFlight);
		for (uint32_t i = 0; i < s_RendererData->FramesInFlight; ++i)
		{
			s_RendererData->CommandBuffers.push_back(s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(false));
			s_RendererData->Fences.push_back(Fence::Create(true));
			s_RendererData->Semaphores.push_back(Semaphore::Create());
		}

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { s_RendererData->ViewportSize, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->FinalImage = Image::Create(colorSpecs, "FinalImage");

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000; // TODO: Check endianess
		Texture2D::WhiteTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &whitePixel);
		Texture2D::WhiteTexture->m_Path = "White";
		Texture2D::BlackTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &blackPixel);
		Texture2D::BlackTexture->m_Path = "Black";
		Texture2D::NoneTexture = Texture2D::Create("assets/textures/Editor/none.png");
		Texture2D::MeshIconTexture = Texture2D::Create("assets/textures/Editor/meshicon.png");
		Texture2D::TextureIconTexture = Texture2D::Create("assets/textures/Editor/textureicon.png");
		Texture2D::SceneIconTexture = Texture2D::Create("assets/textures/Editor/sceneicon.png");
		Texture2D::SoundIconTexture = Texture2D::Create("assets/textures/Editor/soundicon.png");
		Texture2D::FolderIconTexture = Texture2D::Create("assets/textures/Editor/foldericon.png");
		Texture2D::UnknownIconTexture = Texture2D::Create("assets/textures/Editor/unknownicon.png");
		Texture2D::PlayButtonTexture = Texture2D::Create("assets/textures/Editor/playbutton.png");
		Texture2D::StopButtonTexture = Texture2D::Create("assets/textures/Editor/stopbutton.png");

		Sampler::PointSampler = Sampler::Create(FilterMode::Point, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);

		SetupPresentPipeline();

		//Renderer3D Init
		s_RendererData->Sprites.reserve(1'000);
		s_RendererData->Lines.reserve(100);

		//Renderer2D Init
		s_RendererAPI->Init();
		Renderer2D::Init();

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		Renderer::SubmitCommandBuffer(cmd, true);
	}

	void Renderer::Shutdown()
	{
		StagingManager::ReleaseBuffers();
		Renderer2D::Shutdown();
		//TODO: Move to AssetManager::Shutdown()
		TextureLibrary::Clear();
		StaticMeshLibrary::Clear();
		ShaderLibrary::Clear();

		Texture2D::WhiteTexture.reset();
		Texture2D::BlackTexture.reset();
		Texture2D::NoneTexture.reset();
		Texture2D::MeshIconTexture.reset();
		Texture2D::TextureIconTexture.reset();
		Texture2D::SceneIconTexture.reset();
		Texture2D::SoundIconTexture.reset();
		Texture2D::FolderIconTexture.reset();
		Texture2D::UnknownIconTexture.reset();
		Texture2D::PlayButtonTexture.reset();
		Texture2D::StopButtonTexture.reset();
		Sampler::PointSampler.reset();

		s_RendererData->FinalImage.reset();

		s_RendererAPI->Shutdown();
		s_RendererAPI.reset();

		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		fence->Reset();
		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, nullptr, 0, nullptr, 0);
		fence->Wait();

		delete s_RendererData;
		s_RendererData = nullptr;

		for (uint32_t i = 0; i < Renderer::GetConfig().FramesInFlight; ++i)
			Renderer::GetResourceReleaseQueue(i).Execute();
	}

	void Renderer::BeginFrame()
	{
		Renderer::GetResourceReleaseQueue(s_RendererData->CurrentFrameIndex).Execute();
		s_RendererAPI->BeginFrame();
		s_RendererData->ImGuiLayer = &Application::Get().GetImGuiLayer();
	}

	void Renderer::EndFrame()
	{
		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		auto& semaphore = s_RendererData->Semaphores[s_RendererData->CurrentFrameIndex];
		fence->Wait();
		fence->Reset();
		s_RendererAPI->EndFrame();

		RendererData* rendererData = s_RendererData;
		Renderer::Submit([frameIndex = s_RendererData->CurrentFrameIndex, rendererData](Ref<CommandBuffer>& cmd)
		{
			// Copying to present. Drawing UI
			rendererData->PresentPipeline->SetImageSampler(rendererData->FinalImage, Sampler::PointSampler, 0, 0);
			cmd->BeginGraphics(rendererData->PresentPipeline, rendererData->PresentFramebuffers[frameIndex]);
			//cmd->Draw(6, 0);
			(*rendererData->ImGuiLayer)->End(cmd);
			cmd->EndGraphics();
		});

		uint32_t imageIndex = 0;
		auto imageAcquireSemaphore = rendererData->Swapchain->AcquireImage(&imageIndex);

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, imageAcquireSemaphore.get(), 1, semaphore.get(), 1);
		s_RendererData->Swapchain->Present(semaphore);

		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % s_RendererData->FramesInFlight;
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return s_CommandQueue;
	}

	void Renderer::PrepareRendering()
	{
		Renderer::ResetStats();
		Renderer2D::ResetStats();
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::PrepareRendering();
		const glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();

		s_RendererData->ViewPos = cameraComponent.GetWorldTransform().Location;
		//const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Location;
		//s_RendererData->DirectionalLightsVP = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardVector(), glm::vec3(0.f, 1.f, 0.f));
		//s_RendererData->DirectionalLightsVP = s_RendererData->OrthoProjection * s_RendererData->DirectionalLightsVP;
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::PrepareRendering();
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();

		s_RendererData->ViewPos = editorCamera.GetLocation();
		//const glm::vec3& directionalLightPos = directionalLight.GetWorldTransform().Location;
		//s_RendererData->DirectionalLightsVP = glm::lookAt(directionalLightPos, directionalLightPos + directionalLight.GetForwardVector(), glm::vec3(0.f, 1.f, 0.f));
		//s_RendererData->DirectionalLightsVP = s_RendererData->OrthoProjection * s_RendererData->DirectionalLightsVP;
	}

	void Renderer::EndScene()
	{
		//Sorting from front to back
		//std::sort(std::begin(s_RendererData->Sprites), std::end(s_RendererData->Sprites), customSpritesLess);
		//for (auto& mesh : s_BatchData.Meshes)
		//{
		//	auto& meshes = mesh.second;
		//	std::sort(std::begin(meshes), std::end(meshes), customMeshesLess);
		//}

		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		s_RendererData->Stats.RenderingTook = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;

		s_RendererData->Sprites.clear();
		s_RendererData->Lines.clear();
	}

	void Renderer::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		s_ShaderDependencies[shader->GetHash()].Pipelines.push_back(pipeline);
	}

	void Renderer::RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		auto& pipelines = s_ShaderDependencies[shader->GetHash()].Pipelines;
		auto it = std::find(pipelines.begin(), pipelines.end(), pipeline);
		if (it != pipelines.end())
			pipelines.erase(it);
	}

	void Renderer::OnShaderReloaded(size_t hash)
	{
		auto it = s_ShaderDependencies.find(hash);
		if (it != s_ShaderDependencies.end())
		{
			auto& dependencies = it->second;
			for (auto& pipeline : dependencies.Pipelines)
				pipeline->Recreate();
		}
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

			//Save mesh
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

			//Save mesh
		}
	}

	void Renderer::DrawSprite(const SpriteComponent& sprite, int entityID)
	{
		s_RendererData->Sprites.push_back( {&sprite, entityID} );
	}

	void Renderer::DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
	{
		s_RendererData->Lines.push_back( {start, end, color} );
	}

	Ref<CommandBuffer> Renderer::AllocateCommandBuffer(bool bBegin)
	{
		return s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(bBegin);
	}

	void Renderer::SubmitCommandBuffer(Ref<CommandBuffer>& cmd, bool bBlock)
	{
		if (bBlock)
		{
			Ref<Fence> waitFence = Fence::Create();
			s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, waitFence, nullptr, 0, nullptr, 0);
			waitFence->Wait();
		}
		else
		{
			s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, nullptr, 0, nullptr, 0);
		}
		
	}

	void Renderer::WindowResized(uint32_t width, uint32_t height)
	{
		s_RendererData->ViewportSize = { width, height };

		const float aspectRatio = (float)width / (float)height;
		float orthographicSize = 75.f;
		float orthoLeft = -orthographicSize * aspectRatio * 0.5f;
		float orthoRight = orthographicSize * aspectRatio * 0.5f;
		float orthoBottom = -orthographicSize * 0.5f;
		float orthoTop = orthographicSize * 0.5f;

		s_RendererData->OrthoProjection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, -500.f, 500.f);
	}

	float& Renderer::Gamma()
	{
		return s_RendererData->Gamma;
	}

	float& Renderer::Exposure()
	{
		return s_RendererData->Exposure;
	}

	Ref<Image>& Renderer::GetFinalImage()
	{
		return s_RendererData->FinalImage;
	}

	Ref<Framebuffer>& Renderer::GetGFramebuffer()
	{
		static Ref<Framebuffer> temp;
		return temp;
	}

	RenderCommandQueue& Renderer::GetResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}

	Ref<CommandBuffer>& Renderer::GetCurrentFrameCommandBuffer()
	{
		return s_RendererData->CommandBuffers[s_RendererData->CurrentFrameIndex];
	}

	RendererConfig& Renderer::GetConfig()
	{
		static RendererConfig config;
		return config;
	}

	RendererCapabilities& Renderer::GetCapabilities()
	{
		return s_RendererAPI->GetCapabilities();
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return s_RendererData->CurrentFrameIndex;
	}

	Ref<DescriptorManager>& Renderer::GetDescriptorSetManager()
	{
		return s_RendererData->DescriptorManager;
	}

	void* Renderer::GetPresentRenderPassHandle()
	{
		return s_RendererData->PresentPipeline->GetRenderPassHandle();
	}

	void Renderer::ResetStats()
	{
		memset(&s_RendererData->Stats, 0, sizeof(Renderer::Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return s_RendererData->Stats;
	}
}