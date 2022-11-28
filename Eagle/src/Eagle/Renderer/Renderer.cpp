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

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

//TODO: remove this dependency
#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct SMData
	{
		StaticMesh* StaticMesh;
		Transform WorldTransform;
		int entityID;
	};

	struct SpriteData
	{
		const SpriteComponent* Sprite = nullptr;
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
		Ref<Shader> MeshVertexShader;
		Ref<Shader> MeshFragmentShader;
		Ref<Shader> PBRVertexShader;
		Ref<Shader> PBRFragmentShader;
		Ref<PipelineGraphics> PresentPipeline;
		Ref<PipelineGraphics> MeshPipeline;
		Ref<PipelineGraphics> PBRPipeline;
		std::vector<Ref<Framebuffer>> PresentFramebuffers;
		Ref<Image> FinalImage;
		Ref<Image> AlbedoImage;
		Ref<Image> NormalImage;
		Ref<Image> DepthImage;
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;

		Ref<Buffer> AdditionalMeshDataBuffer;
		Ref<Buffer> PointLightsBuffer;
		Ref<Buffer> SpotLightsBuffer;
		Ref<Buffer> DirectionalLightBuffer;
		Ref<Buffer> MaterialBuffer;

		std::vector<CPUMaterial> ShaderMaterials;
		std::vector<PointLight> PointLights;
		std::vector<SpotLight> SpotLights;

		Ref<ImGuiLayer>* ImGuiLayer = nullptr; // Pointer is used just to avoid incrementing counter

		Ref<CommandManager> GraphicsCommandManager;
		std::vector<Ref<CommandBuffer>> CommandBuffers;
		std::vector<Ref<Fence>> Fences;
		std::vector<Ref<Semaphore>> Semaphores;

		glm::mat4 CurrentFrameViewProj = glm::mat4(1.f);
		glm::mat4 OrthoProjection = glm::mat4(1.f);
		glm::vec3 ViewPos = glm::vec3{0.f};
		
		std::vector<SMData> Meshes;
		std::vector<SpriteData> Sprites;
		std::vector<LineData> Lines;

		// Used by the renderer. Stores all textures required for rendering
		std::unordered_map<Ref<Texture>, size_t> UsedTexturesMap; // size_t = index to vector<Ref<Image>>
		std::vector<Ref<Image>> Images;
		std::vector<Ref<Sampler>> Samplers;
		size_t CurrentTextureIndex = 1; // 0 - DummyTexture
		bool bTextureMapChanged = true;

		Renderer::Statistics Stats[Renderer::GetConfig().FramesInFlight];

		float Gamma = 2.2f;
		float Exposure = 1.f;
		glm::uvec2 ViewportSize = glm::uvec2(0, 0);
		uint32_t CurrentFrameIndex = 0;
		uint64_t FrameNumber = 0;

		static constexpr size_t BaseVertexBufferSize = 10 * 1024 * 1024; // 10 MB
		static constexpr size_t BaseIndexBufferSize  = 5 * 1024 * 1024; // 5 MB
		static constexpr size_t BaseMaterialBufferSize = 1024 * 1024; // 1 MB

		static constexpr size_t BaseLightsCount = 10;
		static constexpr size_t BasePointLightsBufferSize = BaseLightsCount * sizeof(PointLight);
		static constexpr size_t BaseSpotLightsBufferSize  = BaseLightsCount * sizeof(SpotLight);

		static constexpr uint32_t FramesInFlight = Renderer::GetConfig().FramesInFlight;
	};

	struct AdditionalMeshData
	{
		alignas(16) glm::mat4 ViewProjection = glm::mat4(1.f);
	} g_AdditionalMeshData;

	struct ShaderDependencies
	{
		std::vector<Ref<Pipeline>> Pipelines;
	};

	static RendererData* s_RendererData = nullptr;
	static RenderCommandQueue s_CommandQueue;
	static RenderCommandQueue s_ResourceFreeQueue[Renderer::GetConfig().FramesInFlight];

	static Scope<RendererAPI> s_RendererAPI;
	static std::unordered_map<size_t, ShaderDependencies> s_ShaderDependencies;

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
		s_RendererData->PresentVertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		s_RendererData->PresentFragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/present.frag", ShaderType::Fragment);

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

	static void SetupMeshPipeline()
	{
		s_RendererData->MeshVertexShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.vert", ShaderType::Vertex);
		s_RendererData->MeshFragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.frag", ShaderType::Fragment);

		const auto& size = s_RendererData->ViewportSize;

		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = { size.x, size.y, 1 };
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment;
		s_RendererData->DepthImage = Image::Create(depthSpecs, "Renderer_DepthImage");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { size.x, size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->AlbedoImage = Image::Create(colorSpecs, "Renderer_AlbedoImage");

		ImageSpecifications normalSpecs;
		normalSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		normalSpecs.Layout = ImageLayoutType::RenderTarget;
		normalSpecs.Size = { size.x, size.y, 1 };
		normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->NormalImage = Image::Create(normalSpecs, "Renderer_NormalImage");

		ColorAttachment colorAttachment;
		colorAttachment.Image = s_RendererData->AlbedoImage;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.bClearEnabled = true;
		
		ColorAttachment normalAttachment;
		normalAttachment.Image = s_RendererData->NormalImage;
		normalAttachment.InitialLayout = ImageLayoutType::Unknown;
		normalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		normalAttachment.bClearEnabled = true;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_RendererData->DepthImage;
		depthAttachment.bClearEnabled = true;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = s_RendererData->MeshVertexShader;
		state.FragmentShader = s_RendererData->MeshFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(normalAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		s_RendererData->MeshPipeline = PipelineGraphics::Create(state);
	}

	static void SetupPBRPipeline()
	{
		s_RendererData->PBRVertexShader = ShaderLibrary::GetOrLoad("assets/shaders/pbr_shade.vert", ShaderType::Vertex);
		s_RendererData->PBRFragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/pbr_shade.frag", ShaderType::Fragment);

		const auto& size = s_RendererData->ViewportSize;

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { size.x, size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->FinalImage = Image::Create(colorSpecs, "Renderer_FinalImage");

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = false;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->FinalImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_RendererData->DepthImage;
		depthAttachment.bClearEnabled = false;
		depthAttachment.bWriteDepth = false;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = s_RendererData->PBRVertexShader;
		state.FragmentShader = s_RendererData->PBRFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None;

		s_RendererData->PBRPipeline = PipelineGraphics::Create(state);
	}

	// Tries to add texture to the system. Returns its index in vector<Ref<Image>> Images
	static size_t AddTexture(const Ref<Texture>& texture)
	{
		if (!texture)
			return 0;

		auto it = s_RendererData->UsedTexturesMap.find(texture);
		if (it == s_RendererData->UsedTexturesMap.end())
		{
			const size_t index = s_RendererData->CurrentTextureIndex;
			s_RendererData->Images[index] = texture->GetImage();
			s_RendererData->Samplers[index] = texture->GetSampler();

			s_RendererData->UsedTexturesMap[texture] = index;
			s_RendererData->CurrentTextureIndex++;
			s_RendererData->bTextureMapChanged = true;
			return index;
		}

		return it->second;
	}

	void Renderer::Init()
	{
		s_RendererData = new RendererData();
		s_RendererData->Swapchain = Application::Get().GetWindow().GetSwapchain();
		s_RendererData->ViewportSize = s_RendererData->Swapchain->GetSize();
		s_RendererAPI = CreateRendererAPI();

		s_RendererData->Swapchain->SetOnSwapchainRecreatedCallback([data = s_RendererData]()
		{
			Application::Get().GetRenderContext()->WaitIdle();
			data->PresentFramebuffers.clear();

			auto& swapchainImages = data->Swapchain->GetImages();
			const void* renderPassHandle = data->PresentPipeline->GetRenderPassHandle();
			glm::uvec2 size = data->Swapchain->GetSize();
			data->ViewportSize = size;
			data->FinalImage->Resize({ size, 1 });
			data->AlbedoImage->Resize({ size, 1 });
			data->NormalImage->Resize({ size, 1 });
			data->DepthImage->Resize({ size, 1 });
			data->MeshPipeline->Resize(size.x, size.y);
			data->PBRPipeline->Resize(size.x, size.y);
			Renderer2D::OnResized(size);
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
		s_RendererData->DescriptorManager = DescriptorManager::Create(DescriptorManager::MaxNumDescriptors, DescriptorManager::MaxSets);

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { s_RendererData->ViewportSize, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000; // TODO: Check endianess
		Texture2D::WhiteTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &whitePixel, {}, false);
		Texture2D::WhiteTexture->m_Path = "White";
		Texture2D::BlackTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &blackPixel, {}, false);
		Texture2D::BlackTexture->m_Path = "Black";
		Texture2D::DummyTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &blackPixel, {}, false);
		Texture2D::DummyTexture->m_Path = "None";
		Texture2D::NoneIconTexture = Texture2D::Create("assets/textures/Editor/none.png");
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
		SetupMeshPipeline();
		SetupPBRPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_RendererData->BaseVertexBufferSize;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_RendererData->BaseIndexBufferSize;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		BufferSpecifications materialsBufferSpecs;
		materialsBufferSpecs.Size = s_RendererData->BaseMaterialBufferSize;
		materialsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications additionalMeshDataBufferSpecs;
		additionalMeshDataBufferSpecs.Size = sizeof(AdditionalMeshData);
		additionalMeshDataBufferSpecs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;

		BufferSpecifications pointLightsBufferSpecs;
		pointLightsBufferSpecs.Size = s_RendererData->BasePointLightsBufferSize;
		pointLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications spotLightsBufferSpecs;
		spotLightsBufferSpecs.Size = s_RendererData->BaseSpotLightsBufferSize;
		spotLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications directionalLightBufferSpecs;
		directionalLightBufferSpecs.Size = sizeof(DirectionalLight);
		directionalLightBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		s_RendererData->VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer");
		s_RendererData->IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer");
		s_RendererData->MaterialBuffer = Buffer::Create(materialsBufferSpecs, "MaterialsBuffer");
		s_RendererData->AdditionalMeshDataBuffer = Buffer::Create(additionalMeshDataBufferSpecs, "AdditionalMeshData");

		s_RendererData->PointLightsBuffer = Buffer::Create(pointLightsBufferSpecs, "PointLightsBuffer");
		s_RendererData->SpotLightsBuffer = Buffer::Create(spotLightsBufferSpecs, "SpotLightsBuffer");
		s_RendererData->DirectionalLightBuffer = Buffer::Create(directionalLightBufferSpecs, "DirectionalLightBuffer");

		//Renderer3D Init
		s_RendererData->Sprites.reserve(1'000);
		s_RendererData->Lines.reserve(100);
		s_RendererData->ShaderMaterials.reserve(100);

		// Init textures & Fill with black textures
		s_RendererData->Images.resize(MAX_TEXTURES);
		s_RendererData->Samplers.resize(MAX_TEXTURES);
		std::fill(s_RendererData->Images.begin(), s_RendererData->Images.end(), Texture2D::DummyTexture->GetImage());
		std::fill(s_RendererData->Samplers.begin(), s_RendererData->Samplers.end(), Texture2D::DummyTexture->GetSampler());

		//Renderer2D Init
		s_RendererAPI->Init();
		Renderer2D::Init(s_RendererData->AlbedoImage, s_RendererData->NormalImage, s_RendererData->DepthImage);

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		Renderer::SubmitCommandBuffer(cmd, true);
	}
	
	bool Renderer::UsedTextureChanged()
	{
		return s_RendererData->bTextureMapChanged;
	}

	size_t Renderer::GetTextureIndex(const Ref<Texture>& texture)
	{
		auto it = s_RendererData->UsedTexturesMap.find(texture);
		if (it == s_RendererData->UsedTexturesMap.end())
			return 0;
		return it->second;
	}

	const std::vector<Ref<Image>>& Renderer::GetUsedImages()
	{
		return s_RendererData->Images;
	}

	const std::vector<Ref<Sampler>>& Renderer::GetUsedSamplers()
	{
		return s_RendererData->Samplers;
	}

	const Ref<Buffer>& Renderer::GetMaterialsBuffer()
	{
		return s_RendererData->MaterialBuffer;
	}

	void Renderer::Shutdown()
	{
		StagingManager::ReleaseBuffers();
		Renderer2D::Shutdown();
		//TODO: Move to AssetManager::Shutdown()
		TextureLibrary::Clear();
		StaticMeshLibrary::Clear();
		ShaderLibrary::Clear();

		Texture2D::DummyTexture.reset();
		Texture2D::WhiteTexture.reset();
		Texture2D::BlackTexture.reset();
		Texture2D::NoneIconTexture.reset();
		Texture2D::MeshIconTexture.reset();
		Texture2D::TextureIconTexture.reset();
		Texture2D::SceneIconTexture.reset();
		Texture2D::SoundIconTexture.reset();
		Texture2D::FolderIconTexture.reset();
		Texture2D::UnknownIconTexture.reset();
		Texture2D::PlayButtonTexture.reset();
		Texture2D::StopButtonTexture.reset();
		Sampler::PointSampler.reset();

		s_RendererData->AlbedoImage.reset();
		s_RendererData->DepthImage.reset();

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
		StagingManager::NextFrame();
		s_RendererAPI->BeginFrame();
		s_RendererData->ImGuiLayer = &Application::Get().GetImGuiLayer();
	}

	void Renderer::EndFrame()
	{
		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		auto& semaphore = s_RendererData->Semaphores[s_RendererData->CurrentFrameIndex];
		fence->Wait();
		s_RendererAPI->EndFrame();

		uint32_t imageIndex = 0;
		auto imageAcquireSemaphore = s_RendererData->Swapchain->AcquireImage(&imageIndex);

		Renderer::Submit([imageIndex, data = s_RendererData](Ref<CommandBuffer>& cmd)
		{
			// Drawing UI if build with editor. Otherwise just copy final render to present
			{
#ifndef EG_WITH_EDITOR
				data->PresentPipeline->SetImageSampler(dataAlbedoImage, Sampler::PointSampler, 0, 0);
#endif
				cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[imageIndex]);
#ifndef EG_WITH_EDITOR
				cmd->Draw(6, 0);
#else
				(*data->ImGuiLayer)->End(cmd);
#endif
				cmd->EndGraphics();
			}
		});

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		fence->Reset();
		s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, imageAcquireSemaphore.get(), 1, semaphore.get(), 1);
		s_RendererData->Swapchain->Present(semaphore);

		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % s_RendererData->FramesInFlight;
		s_RendererData->FrameNumber++;
		s_RendererData->bTextureMapChanged = false;

		// Reset stats of the next frame
		Renderer::ResetStats();
		Renderer2D::ResetStats();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return s_CommandQueue;
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4 cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();

		s_RendererData->CurrentFrameViewProj = cameraProjection * cameraView;
		s_RendererData->ViewPos = cameraComponent.GetWorldTransform().Location;
		UpdateLightsBuffers(pointLights, directionalLight, spotLights);
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();

		s_RendererData->CurrentFrameViewProj = cameraProjection * cameraView;
		s_RendererData->ViewPos = editorCamera.GetLocation();
		UpdateLightsBuffers(pointLights, directionalLight, spotLights);
	}

	void Renderer::UpdateLightsBuffers(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLightComponent, const std::vector<SpotLightComponent*>& spotLights)
	{
		Renderer::Submit([&](Ref<CommandBuffer>& cmd)
		{
			s_RendererData->PointLights.clear();
			s_RendererData->SpotLights.clear();

			constexpr glm::vec3 directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
							glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),  glm::vec3(0.0, 0.0,-1.0) };
			constexpr glm::vec3 upVectors[6] = { glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),
												 glm::vec3(0.0, 0.0,-1.0), glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0,-1.0, 0.0) };
			const glm::mat4 pointLightPerspectiveProjection = glm::perspective(glm::radians(90.f), 1.f, 0.01f, 10000.f);

			for (auto& pointLight : pointLights)
			{
				PointLight light;
				light.Position  = pointLight->GetWorldTransform().Location;
				light.Ambient   = pointLight->Ambient;
				light.Diffuse   = pointLight->LightColor;
				light.Specular  = pointLight->Specular;
				light.Intensity = pointLight->Intensity;

				for (int i = 0; i < 6; ++i)
					light.ViewProj[i] = pointLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + directions[i], upVectors[i]);

				s_RendererData->PointLights.push_back(light);
			}

			for (auto& spotLight : spotLights)
			{
				SpotLight light;
				light.Position = spotLight->GetWorldTransform().Location;
				light.Direction = spotLight->GetForwardVector();

				light.Ambient = spotLight->Ambient;
				light.Diffuse = spotLight->LightColor;
				light.Specular = spotLight->Specular;
				light.InnerCutOffAngle = spotLight->InnerCutOffAngle;
				light.OuterCutOffAngle = spotLight->OuterCutOffAngle;
				light.Intensity = spotLight->Intensity;

				const glm::mat4 spotLightPerspectiveProjection = glm::perspective(glm::radians(glm::min(light.OuterCutOffAngle * 2.f, 179.f)), 1.f, 0.01f, 10000.f);
				light.ViewProj = spotLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + light.Direction, glm::vec3{ 0.f, 1.f, 0.f });

				s_RendererData->SpotLights.push_back(light);
			}

			DirectionalLight directionalLight;
			directionalLight.Direction = directionalLightComponent.GetForwardVector();
			directionalLight.Ambient = directionalLightComponent.Ambient;
			directionalLight.Diffuse = directionalLightComponent.LightColor;
			directionalLight.Specular = directionalLightComponent.Specular;

			const glm::vec3& directionalLightPos = directionalLightComponent.GetWorldTransform().Location;
			directionalLight.ViewProj = glm::lookAt(directionalLightPos, directionalLightPos + directionalLightComponent.GetForwardVector(), glm::vec3(0.f, 1.f, 0.f));
			directionalLight.ViewProj = s_RendererData->OrthoProjection * directionalLight.ViewProj;

			const size_t pointLightsDataSize = s_RendererData->PointLights.size() * sizeof(PointLight);
			const size_t spotLightsDataSize = s_RendererData->SpotLights.size() * sizeof(SpotLight);
			if (pointLightsDataSize > s_RendererData->PointLightsBuffer->GetSize())
				s_RendererData->PointLightsBuffer->Resize((pointLightsDataSize * 3) / 2);
			if (spotLightsDataSize > s_RendererData->SpotLightsBuffer->GetSize())
				s_RendererData->SpotLightsBuffer->Resize((spotLightsDataSize * 3) / 2);

			if (pointLightsDataSize)
				cmd->Write(s_RendererData->PointLightsBuffer, s_RendererData->PointLights.data(), pointLightsDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			if (spotLightsDataSize)
				cmd->Write(s_RendererData->SpotLightsBuffer, s_RendererData->SpotLights.data(), spotLightsDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
			cmd->Write(s_RendererData->DirectionalLightBuffer, &directionalLight, sizeof(DirectionalLight), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->PointLightsBuffer, EG_PBR_SET, EG_BINDING_POINT_LIGHTS);
			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->SpotLightsBuffer, EG_PBR_SET, EG_BINDING_SPOT_LIGHTS);
			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->DirectionalLightBuffer, EG_PBR_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		});
	}

	void Renderer::EndScene()
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

		//Sorting from front to back
		std::sort(std::begin(s_RendererData->Sprites), std::end(s_RendererData->Sprites), customSpritesLess);
		std::sort(std::begin(s_RendererData->Meshes), std::end(s_RendererData->Meshes), customMeshesLess);

		s_RendererData->ShaderMaterials.clear();
		for (auto& mesh : s_RendererData->Meshes)
		{
			uint32_t diffuseTextureIndex  = (uint32_t)AddTexture(mesh.StaticMesh->Material->GetDiffuseTexture());
			uint32_t specularTextureIndex = (uint32_t)AddTexture(mesh.StaticMesh->Material->GetSpecularTexture());
			uint32_t normalTextureIndex   = (uint32_t)AddTexture(mesh.StaticMesh->Material->GetNormalTexture());

			CPUMaterial material;
			material.PackedTextureIndices = 0;
			material.PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
			material.PackedTextureIndices |= (specularTextureIndex << SpecularTextureOffset);
			material.PackedTextureIndices |= diffuseTextureIndex;

			s_RendererData->ShaderMaterials.push_back(material);
		}
		for (auto& sprite : s_RendererData->Sprites)
		{
			AddTexture(sprite.Sprite->Material->GetDiffuseTexture());
			AddTexture(sprite.Sprite->Material->GetSpecularTexture());
			AddTexture(sprite.Sprite->Material->GetNormalTexture());
		}
		if (s_RendererData->bTextureMapChanged)
		{
			s_RendererData->MeshPipeline->SetImageSamplerArray(s_RendererData->Images, s_RendererData->Samplers, EG_PERSISTENT_SET, 0);
		}

		RenderMeshes();
		RenderSprites();
		PBRPass();

		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		s_RendererData->Stats[s_RendererData->CurrentFrameIndex].RenderingTook = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;

		s_RendererData->Meshes.clear();
		s_RendererData->Sprites.clear();
		s_RendererData->Lines.clear();
	}

	void Renderer::PBRPass()
	{
		Renderer::Submit([data = s_RendererData](Ref<CommandBuffer>& cmd)
		{
			struct PushData
			{
				glm::vec3 ViewPosition;
			} pushData;
			pushData.ViewPosition = s_RendererData->ViewPos;

			data->PBRPipeline->SetImageSampler(data->AlbedoImage, Sampler::PointSampler, EG_PBR_SET, EG_BINDING_ALBEDO_TEXTURE);
			data->PBRPipeline->SetImageSampler(data->NormalImage, Sampler::PointSampler, EG_PBR_SET, EG_BINDING_NORMAL_TEXTURE);

			cmd->BeginGraphics(data->PBRPipeline);
			cmd->SetGraphicsRootConstants(nullptr, &pushData);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
		});
	}

	void Renderer::RenderMeshes()
	{
		if (s_RendererData->Meshes.empty())
			return;

		Renderer::Submit([meshes = std::move(s_RendererData->Meshes), materials = std::move(s_RendererData->ShaderMaterials)](Ref<CommandBuffer>& cmd)
		{
			const size_t materilBufferSize = s_RendererData->MaterialBuffer->GetSize();
			const size_t materialDataSize = materials.size() * sizeof(CPUMaterial);

			if (materialDataSize > materilBufferSize)
				s_RendererData->MaterialBuffer->Resize((materilBufferSize * 3) / 2);

			s_RendererData->MeshPipeline->SetBuffer(s_RendererData->MaterialBuffer, EG_PERSISTENT_SET, 1);
			cmd->Write(s_RendererData->MaterialBuffer, materials.data(), materialDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

			struct VertexPushData
			{
				glm::mat4 Model = glm::mat4(1.f);
				uint32_t MaterialIndex = 0;
			} pushData;

			// Reserving enough space to hold Vertex & Index data
			size_t currentVertexSize = 0;
			size_t currentIndexSize = 0;
			for (auto& mesh : meshes)
			{
				size_t verticesSize = mesh.StaticMesh->GetVerticesCount() * sizeof(Vertex);
				size_t indicesSize = mesh.StaticMesh->GetIndecesCount() * sizeof(Index);
				if (verticesSize > currentVertexSize)
					currentVertexSize = verticesSize;
				if (indicesSize > currentIndexSize)
					currentIndexSize = indicesSize;
			}
			if (currentVertexSize > s_RendererData->VertexBuffer->GetSize())
				s_RendererData->VertexBuffer->Resize(currentVertexSize);
			if (currentIndexSize > s_RendererData->IndexBuffer->GetSize())
				s_RendererData->IndexBuffer->Resize(currentIndexSize);

			static std::vector<Vertex> vertices;
			static std::vector<Index> indices;
			vertices.clear(); vertices.reserve(currentVertexSize);
			indices.clear(); indices.reserve(currentIndexSize);

			for (auto& mesh : meshes)
			{
				const auto& meshVertices = mesh.StaticMesh->GetVertices();
				const auto& meshIndices = mesh.StaticMesh->GetIndeces();
				vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
				indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());
			}

			auto& vb = s_RendererData->VertexBuffer;
			auto& ib = s_RendererData->IndexBuffer;
			auto& additionalBuffer = s_RendererData->AdditionalMeshDataBuffer;

			cmd->Write(vb, vertices.data(), vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
			cmd->Write(ib, indices.data(), indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);

			g_AdditionalMeshData.ViewProjection = s_RendererData->CurrentFrameViewProj;
			cmd->Write(additionalBuffer, &g_AdditionalMeshData, sizeof(AdditionalMeshData), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
			s_RendererData->MeshPipeline->SetBuffer(additionalBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

			cmd->BeginGraphics(s_RendererData->MeshPipeline);
			uint32_t firstIndex = 0;
			uint32_t vertexOffset = 0;

			uint32_t meshIndex = 0;
			for (auto& mesh : meshes)
			{
				const auto& vertices = mesh.StaticMesh->GetVertices();
				const auto& indices = mesh.StaticMesh->GetIndeces();
				size_t vertexSize = vertices.size() * sizeof(Vertex);
				size_t indexSize = indices.size() * sizeof(Index);

				pushData.Model = Math::ToTransformMatrix(mesh.WorldTransform);
				pushData.MaterialIndex = meshIndex;

				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->DrawIndexed(vb, ib, (uint32_t)indices.size(), firstIndex, vertexOffset);
				firstIndex += (uint32_t)indices.size();
				vertexOffset += (uint32_t)vertices.size();
				meshIndex++;

				s_RendererData->Stats[s_RendererData->CurrentFrameIndex].DrawCalls++;
				s_RendererData->Stats[s_RendererData->CurrentFrameIndex].Vertices += (uint32_t)vertices.size();
				s_RendererData->Stats[s_RendererData->CurrentFrameIndex].Indeces += (uint32_t)indices.size();
			}
			cmd->EndGraphics();
		});
	}

	void Renderer::RenderSprites()
	{
		auto& sprites = s_RendererData->Sprites;

		if (sprites.empty())
			return;

		Renderer2D::BeginScene(s_RendererData->CurrentFrameViewProj);
		for(auto& spriteData : sprites)
		{
			Renderer2D::DrawQuad(*spriteData.Sprite);
		}
		Renderer2D::EndScene();
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

	void Renderer::DrawMesh(const StaticMeshComponent& smComponent)
	{
		const Ref<Eagle::StaticMesh>& staticMesh = smComponent.StaticMesh;
		if (staticMesh)
		{
			size_t verticesCount = staticMesh->GetVerticesCount();
			size_t indecesCount = staticMesh->GetIndecesCount();

			if (verticesCount == 0 || indecesCount == 0)
				return;

			//Save mesh
			SMData data;
			data.StaticMesh = staticMesh.get();
			data.WorldTransform = smComponent.GetWorldTransform();
			data.entityID = smComponent.Parent;
			s_RendererData->Meshes.push_back(data);
		}
	}

	void Renderer::DrawSprite(const SpriteComponent& sprite)
	{
		s_RendererData->Sprites.push_back( {&sprite} );
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

	Ref<Image>& Renderer::GetNormalsImage()
	{
		return s_RendererData->NormalImage;
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

	const Ref<PipelineGraphics>& Renderer::GetMeshPipeline()
	{
		return s_RendererData->MeshPipeline;
	}

	void* Renderer::GetPresentRenderPassHandle()
	{
		return s_RendererData->PresentPipeline->GetRenderPassHandle();
	}

	uint64_t Renderer::GetFrameNumber()
	{
		return s_RendererData->FrameNumber;
	}

	void Renderer::ResetStats()
	{
		memset(&s_RendererData->Stats[s_RendererData->CurrentFrameIndex], 0, sizeof(Renderer::Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		uint32_t index = s_RendererData->CurrentFrameIndex;
		index = index == 0 ? s_RendererData->FramesInFlight - 2 : index - 1;

		return s_RendererData->Stats[index]; // Returns stats of the prev frame because current frame stats are not ready yet
	}
}
