#include "egpch.h"

#include "RenderManager.h"
#include "TextureSystem.h"
#include "MaterialSystem.h"

#include "VidWrappers/Buffer.h"
#include "VidWrappers/Framebuffer.h"
#include "VidWrappers/PipelineGraphics.h"
#include "VidWrappers/StagingManager.h"
#include "VidWrappers/RenderCommandManager.h"
#include "VidWrappers/Fence.h"
#include "VidWrappers/Semaphore.h"
#include "VidWrappers/Texture.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Classes/StaticMesh.h"

namespace Eagle
{
#ifdef EG_WITH_EDITOR
	std::mutex g_ImGuiMutex;
#endif
	std::mutex g_TimingsMutex;

	struct RendererData
	{
		ThreadPool ThreadPool{"Render Thread", 1};
		std::array<std::future<void>, RendererConfig::FramesInFlight> ThreadPoolTasks;

		Ref<DescriptorManager> DescriptorManager;
		Ref<VulkanSwapchain> Swapchain;

		Ref<PipelineGraphics> PresentPipeline;
		Ref<PipelineGraphics> IBLPipeline;
		Ref<PipelineGraphics> IrradiancePipeline;
		Ref<PipelineGraphics> PrefilterPipeline;
		Ref<PipelineGraphics> BRDFLUTPipeline;
		std::vector<Ref<Framebuffer>> PresentFramebuffers;

		Ref<ImGuiLayer>* ImGuiLayer = nullptr; // Pointer is used just to avoid incrementing counter

		Ref<CommandManager> GraphicsCommandManager;
		std::vector<Ref<CommandBuffer>> CommandBuffers;
		std::vector<Ref<Fence>> Fences;
		std::vector<Ref<Semaphore>> Semaphores;

		Ref<Image> DummyImage;
		Ref<Image> DummyImageCube;
		Ref<Image> DummyImageR16;
		Ref<Image> DummyImageR16Cube;
		Ref<Image> DummyImage3D;
		Ref<Image> DummyRGBA16FImage;
		Ref<Image> DummyDepthImage;
		Ref<Image> DummyCubeDepthImage;
		Ref<Image> BRDFLUTImage;
		Ref<TextureCube> DummyIBL;
		Ref<TextureCube> IBLTexture;

		GPUTimingsContainer GPUTimings; // Sorted
#ifdef EG_GPU_TIMINGS
		std::unordered_map<std::string_view, Ref<RHIGPUTiming>> RHIGPUTimings;
		std::unordered_map<std::string_view, Weak<RHIGPUTiming>> RHIGPUTimingsParentless; // Timings that do not have parents
#endif

		glm::vec2 HaltonSequence[s_JitterSize];

		uint32_t SwapchainImageIndex = 0;
		uint32_t CurrentRenderingFrameIndex = 0;
		uint32_t CurrentFrameIndex = 0;
		uint32_t CurrentReleaseFrameIndex = 0;
		uint64_t FrameNumber = 0;
	};

	struct ShaderDependencies
	{
		std::vector<Weak<Pipeline>> Pipelines;
	};

	static RendererData* s_RendererData = nullptr;
	bool RenderManager::bImmediateDeletionMode = false;

	static RenderCommandQueue s_CommandQueue[RendererConfig::FramesInFlight];
	static RenderCommandQueue s_ResourceFreeQueue[RendererConfig::ReleaseFramesInFlight];

	// We never access `Shader` so it's fine to hold raw pointer to it
	static std::unordered_map<const Shader*, ShaderDependencies> s_ShaderDependencies;

#ifdef EG_GPU_TIMINGS
	struct {
		bool operator()(const GPUTimingData& a, const GPUTimingData& b) const
		{
			return a.Timing > b.Timing;
		}
	} s_CustomGPUTimingsLess;
#endif

	static void UpdateGPUTimings()
	{
		// First, we need to deinit timings before releasing the memory
		// Because they can point to each other and if one of the dies, the other might try to access it during deinitialization
		for (auto it = s_RendererData->RHIGPUTimings.begin(); it != s_RendererData->RHIGPUTimings.end(); ++it)
		{
			if (it->second->bIsUsed == false)
				it->second->Destroy();
		}

		for (auto it = s_RendererData->RHIGPUTimings.begin(); it != s_RendererData->RHIGPUTimings.end();)
		{
			// If it wasn't used in prev frame, erase it
			if (it->second->bIsUsed == false)
				it = s_RendererData->RHIGPUTimings.erase(it);
			else
			{
				it->second->QueryTiming(s_RendererData->CurrentRenderingFrameIndex);
				it->second->bIsUsed = false; // Set to false for the current frame
				++it;
			}
		}
	}

	static GPUTimingData ProcessTimingChildren(const RHIGPUTiming* timing)
	{
		GPUTimingData data;
		data.Name = timing->GetName();
		data.Timing = timing->GetTiming();

		const auto& children = timing->GetChildren();
		for (auto& child : children)
		{
			auto& childData = data.Children.emplace_back();
			childData.Name = child->GetName();
			childData.Timing = child->GetTiming();
		}

		const size_t childsCount = children.size();
		for (size_t i = 0; i < childsCount; ++i)
		{
			auto& child = children[i];
			for (auto& childsChild : child->GetChildren())
				data.Children[i].Children.push_back(ProcessTimingChildren(childsChild));
			std::sort(data.Children[i].Children.begin(), data.Children[i].Children.end(), s_CustomGPUTimingsLess);
		}
		std::sort(data.Children.begin(), data.Children.end(), s_CustomGPUTimingsLess);

		return data;
	}

	static void SortGPUTimings()
	{
#ifdef EG_GPU_TIMINGS
		s_RendererData->GPUTimings.clear();
		for (auto it = s_RendererData->RHIGPUTimingsParentless.begin(); it != s_RendererData->RHIGPUTimingsParentless.end();)
		{
			if (it->second.expired())
			{
				it = s_RendererData->RHIGPUTimingsParentless.erase(it);
				continue;
			}

			Ref<RHIGPUTiming> timing = it->second.lock();
			auto& lastTiming = s_RendererData->GPUTimings.emplace_back(ProcessTimingChildren(timing.get()));
			++it;
		}

		std::sort(s_RendererData->GPUTimings.begin(), s_RendererData->GPUTimings.end(), s_CustomGPUTimingsLess);
#endif
	}

	static void InitHaltonSequence()
	{
		constexpr float scale = 1.f;
		for (uint32_t i = 0; i < s_JitterSize; ++i)
		{
			s_RendererData->HaltonSequence[i].x = scale * CreateHaltonSequence(i + 1u, 2u);
			s_RendererData->HaltonSequence[i].y = scale * CreateHaltonSequence(i + 1u, 3u);
		}
	}

	static Ref<Image> CreateDepthImage(glm::uvec3 size, std::string_view debugName, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		depthSpecs.bIsCube = bCube;
		depthSpecs.Size = size;
		return Image::Create(depthSpecs, debugName.data());
	}

	static void SetupPresentPipeline()
	{
		auto& swapchainImages = s_RendererData->Swapchain->GetImages();
		const auto& size = swapchainImages[0]->GetSize();

		ColorAttachment colorAttachment;
		colorAttachment.Image = swapchainImages[0];
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageLayoutType::Present;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/present.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		s_RendererData->PresentPipeline = PipelineGraphics::Create(state);

		for (auto& image : swapchainImages)
			s_RendererData->PresentFramebuffers.push_back(Framebuffer::Create({ image }, size, s_RendererData->PresentPipeline->GetRenderPassHandle()));
	}

	static void SetupIBLPipeline()
	{
		auto vertexShader = ShaderLibrary::GetOrLoad("assets/shaders/ibl.vert", ShaderType::Vertex);

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->DummyRGBA16FImage; // just a dummy here

		PipelineGraphicsState state;
		state.VertexShader = vertexShader;
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/ibl.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.Size = { TextureCube::SkyboxSize, TextureCube::SkyboxSize };
		state.bImagelessFramebuffer = true;

		PipelineGraphicsState irradianceState;
		irradianceState.VertexShader = vertexShader;
		irradianceState.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/generate_irradiance.frag", ShaderType::Fragment);
		irradianceState.ColorAttachments.push_back(colorAttachment);
		irradianceState.Size = { TextureCube::IrradianceSize, TextureCube::IrradianceSize };
		irradianceState.bImagelessFramebuffer = true;

		PipelineGraphicsState prefilterState;
		prefilterState.VertexShader = vertexShader;
		prefilterState.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/prefilter_ibl.frag", ShaderType::Fragment);
		prefilterState.ColorAttachments.push_back(colorAttachment);
		prefilterState.Size = { TextureCube::PrefilterSize, TextureCube::PrefilterSize };
		prefilterState.bImagelessFramebuffer = true;

		s_RendererData->IBLPipeline = PipelineGraphics::Create(state);
		s_RendererData->IrradiancePipeline = PipelineGraphics::Create(irradianceState);
		s_RendererData->PrefilterPipeline = PipelineGraphics::Create(prefilterState);
	}

	static void SetupBRDFLUTPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->BRDFLUTImage;

		PipelineGraphicsState brdfLutState;
		brdfLutState.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		brdfLutState.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/brdf_lut.frag", ShaderType::Fragment);
		brdfLutState.ColorAttachments.push_back(colorAttachment);
		brdfLutState.Size = { RendererConfig::BRDFLUTSize, RendererConfig::BRDFLUTSize };

		s_RendererData->BRDFLUTPipeline = PipelineGraphics::Create(brdfLutState);
	}

	void RenderManager::Init()
	{
		s_RendererData = new RendererData();
		s_RendererData->Swapchain = Application::Get().GetWindow().GetSwapchain();

		InitHaltonSequence();

		s_RendererData->Swapchain->SetOnSwapchainRecreatedCallback([data = s_RendererData]()
		{
			data->PresentFramebuffers.clear();
			auto& swapchainImages = data->Swapchain->GetImages();
			glm::uvec2 size = s_RendererData->Swapchain->GetSize();
			const void* renderPassHandle = data->PresentPipeline->GetRenderPassHandle();
			for (auto& image : swapchainImages)
				data->PresentFramebuffers.push_back(Framebuffer::Create({ image }, size, data->PresentPipeline->GetRenderPassHandle()));
		});

		s_RendererData->GraphicsCommandManager = CommandManager::Create(CommandQueueFamily::Graphics, true);
		s_RendererData->CommandBuffers.reserve(RendererConfig::FramesInFlight);
		s_RendererData->Fences.reserve(RendererConfig::FramesInFlight);
		s_RendererData->Semaphores.reserve(RendererConfig::FramesInFlight);
		for (uint32_t i = 0; i < RendererConfig::FramesInFlight; ++i)
		{
			s_RendererData->CommandBuffers.push_back(s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(false));
			s_RendererData->Fences.push_back(Fence::Create(true));
			s_RendererData->Semaphores.push_back(Semaphore::Create());
		}
		s_RendererData->DescriptorManager = DescriptorManager::Create(DescriptorManager::MaxNumDescriptors, DescriptorManager::MaxSets);

		const uint32_t whitePixel = 0xffffffff;
		const uint32_t blackPixel = 0xff000000;
		const uint32_t half = 0xff7f7f7f;
		const uint32_t red = 0xff0000ff;
		const uint32_t green = 0xff00ff00;
		const uint32_t blue = 0xffff0000;
		Texture2D::WhiteTexture = Texture2D::Create("White", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &whitePixel, {}, false);
		Texture2D::BlackTexture = Texture2D::Create("Black", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blackPixel, {}, false);
		Texture2D::GrayTexture = Texture2D::Create("Gray", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &half, {}, false);
		Texture2D::RedTexture = Texture2D::Create("Red", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &red, {}, false);
		Texture2D::GreenTexture = Texture2D::Create("Green", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &green, {}, false);
		Texture2D::BlueTexture = Texture2D::Create("Blue", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blue, {}, false);
		Texture2D::DummyTexture = Texture2D::Create("None", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blackPixel, {}, false);
		Texture2D::NoneIconTexture = Texture2D::Create("assets/textures/Editor/none.png", {}, false);
		Texture2D::PointLightIcon = Texture2D::Create("assets/textures/Editor/pointlight.png", {}, false);
		Texture2D::DirectionalLightIcon = Texture2D::Create("assets/textures/Editor/directionallight.png", {}, false);
		Texture2D::SpotLightIcon = Texture2D::Create("assets/textures/Editor/spotlight.png", {}, false);

		BufferSpecifications dummyBufferSpecs;
		dummyBufferSpecs.Size = 4;
		dummyBufferSpecs.Usage = BufferUsage::StorageBuffer;
		dummyBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		Buffer::Dummy = Buffer::Create(dummyBufferSpecs, "DummyBuffer");

		Sampler::PointSampler = Sampler::Create(FilterMode::Point, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::PointSamplerClamp = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::BilinearSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::BilinearSamplerClamp = Sampler::Create(FilterMode::Bilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::TrilinearSampler = Sampler::Create(FilterMode::Trilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R16G16B16A16_Float;
		colorSpecs.Layout = ImageLayoutType::Unknown;
		colorSpecs.Size = { 1, 1, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->DummyRGBA16FImage = Image::Create(colorSpecs, "DummyRGBA16F");

		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		s_RendererData->DummyImage = Image::Create(colorSpecs, "Dummy2D");

		colorSpecs.Format = ImageFormat::R16_Float;
		s_RendererData->DummyImageR16 = Image::Create(colorSpecs, "Dummy2D_R16");
		
		colorSpecs.bIsCube = true;
		s_RendererData->DummyImageR16Cube = Image::Create(colorSpecs, "Dummy2D_R16_Cube");

		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		s_RendererData->DummyImageCube = Image::Create(colorSpecs, "DummyCube");
		colorSpecs.bIsCube = false;

		colorSpecs.Type = ImageType::Type3D;
		s_RendererData->DummyImage3D = Image::Create(colorSpecs, "Dummy3D");
		colorSpecs.Type = ImageType::Type2D;

		{
			colorSpecs.Size = { RendererConfig::BRDFLUTSize, RendererConfig::BRDFLUTSize, 1 };
			colorSpecs.Format = ImageFormat::R16G16_Float;
			s_RendererData->BRDFLUTImage = Image::Create(colorSpecs, "BRDFLUT_Image");
		}

		s_RendererData->DummyCubeDepthImage = CreateDepthImage(glm::uvec3{ 1, 1, 1 }, "DummyDepthImage_Cube", true);
		s_RendererData->DummyDepthImage = CreateDepthImage(glm::uvec3{ 1, 1, 1 }, "DummyDepthImage", false);

		MaterialSystem::Init();
		TextureSystem::Init();
		// Init renderer pipelines
		SetupPresentPipeline();
		SetupIBLPipeline();
		SetupBRDFLUTPipeline();

		s_RendererData->DummyIBL = TextureCube::Create(Texture2D::BlackTexture, 1, false);

		RenderManager::Submit([](Ref<CommandBuffer>& cmd)
		{
			cmd->TransitionLayout(s_RendererData->DummyDepthImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyCubeDepthImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyImageCube, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyImageR16, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyImageR16Cube, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});

		// Render BRDF LUT
		{
			RenderManager::Submit([](Ref<CommandBuffer>& cmd)
			{
				struct PushData
				{
					uint32_t FlipX = 0;
					uint32_t FlipY = 1;
				} pushData;
				cmd->BeginGraphics(s_RendererData->BRDFLUTPipeline);
				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->Draw(6, 0);
				cmd->EndGraphics();
			});
		}

		auto& cmd = s_RendererData->CommandBuffers[0];
		cmd->Begin();
		s_CommandQueue[0].Execute();
		cmd->End();
		RenderManager::SubmitCommandBuffer(cmd, true);
		s_RendererData->CommandBuffers[0] = s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(false);
	}

	void RenderManager::Finish()
	{
		Wait();

		s_RendererData->ThreadPool->wait_for_tasks();
		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		fence->Reset();
		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		for (uint32_t i = 0; i < RendererConfig::FramesInFlight; ++i)
			s_CommandQueue[i].Execute();
		cmd->End();
		s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, nullptr, 0, nullptr, 0);
		fence->Wait();

		Wait();
	}

	Ref<Image>& RenderManager::GetDummyDepthCubeImage()
	{
		return s_RendererData->DummyCubeDepthImage;
	}

	Ref<Image>& RenderManager::GetDummyDepthImage()
	{
		return s_RendererData->DummyDepthImage;
	}

	Ref<TextureCube>& RenderManager::GetDummyIBL()
	{
		return s_RendererData->DummyIBL;
	}

	const Ref<Image>& RenderManager::GetBRDFLUTImage()
	{
		return s_RendererData->BRDFLUTImage;
	}

	const Ref<Image>& RenderManager::GetDummyImage()
	{
		return s_RendererData->DummyImage;
	}

	const Ref<Image>& RenderManager::GetDummyImageCube()
	{
		return s_RendererData->DummyImageCube;
	}

	const Ref<Image>& RenderManager::GetDummyImageR16()
	{
		return s_RendererData->DummyImageR16;
	}

	const Ref<Image>& RenderManager::GetDummyImageR16Cube()
	{
		return s_RendererData->DummyImageR16Cube;
	}

	const Ref<Image>& RenderManager::GetDummyImage3D()
	{
		return s_RendererData->DummyImage3D;
	}

	const glm::vec2 RenderManager::GetHalton(uint32_t index)
	{
		EG_ASSERT(index < s_JitterSize);
		return s_RendererData->HaltonSequence[index];
	}

	void RenderManager::Shutdown()
	{
#ifdef EG_GPU_TIMINGS
		for (auto& [unused, timing] : s_RendererData->RHIGPUTimings)
			timing->SetParent(nullptr);
		s_RendererData->RHIGPUTimings.clear();
#endif
		s_RendererData->GPUTimings.clear();
		s_ShaderDependencies.clear();

		StagingManager::ReleaseBuffers();
		//TODO: Move to AssetManager::Shutdown()
		TextureLibrary::Clear();
		StaticMeshLibrary::Clear();
		ShaderLibrary::Clear();

		Texture2D::DummyTexture.reset();
		Texture2D::WhiteTexture.reset();
		Texture2D::BlackTexture.reset();
		Texture2D::GrayTexture.reset();
		Texture2D::RedTexture.reset();
		Texture2D::GreenTexture.reset();
		Texture2D::BlueTexture.reset();
		Texture2D::NoneIconTexture.reset();
		Texture2D::PointLightIcon.reset();
		Texture2D::DirectionalLightIcon.reset();
		Texture2D::SpotLightIcon.reset();
		Buffer::Dummy.reset();

		Sampler::PointSampler.reset();
		Sampler::PointSamplerClamp.reset();
		Sampler::BilinearSampler.reset();
		Sampler::BilinearSamplerClamp.reset();
		Sampler::TrilinearSampler.reset();
		TextureSystem::Shutdown();
		MaterialSystem::Shutdown();

		Finish();

		delete s_RendererData;
		s_RendererData = nullptr;

		ReleasePendingResources();
	}

	void RenderManager::Wait()
	{
		for (auto& task : s_RendererData->ThreadPoolTasks)
			if (task.valid())
				task.wait();
		Application::Get().GetRenderContext()->WaitIdle();
	}

	void RenderManager::ReleasePendingResources()
	{
		for (uint32_t i = 0; i < RendererConfig::ReleaseFramesInFlight; ++i)
			RenderManager::GetResourceReleaseQueue(i).Execute();
	}

	void RenderManager::BeginFrame()
	{
		{
			// Here we're waiting to the frame to be executed (waiting for fence)
			// Then we're waiting for all the submissions to finish. Otherwise we'll be lagging behind since frames are being queued up
			// If we're won't wait for all the submission to finish, it's kinda like we're creating our own VSync where we can be behind for up to `FramesInFlight` frames
			EG_CPU_TIMING_SCOPED("Waiting For GPU");
			auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
			fence->Wait();
			for(auto& task : s_RendererData->ThreadPoolTasks)
				if (task.valid())
					task.wait();
		}

		s_RendererData->ImGuiLayer = &Application::Get().GetImGuiLayer();
	}

	void RenderManager::EndFrame()
	{
		RenderManager::Submit([](Ref<CommandBuffer>& cmd)
		{
			struct PushData
			{
				uint32_t FlipX = 0;
				uint32_t FlipY = 0;
			} pushData;

			// Drawing UI if build with editor. Otherwise just copy final render to present
#ifdef EG_WITH_EDITOR
			EG_GPU_TIMING_SCOPED(cmd, "Present+ImGui");

			auto data = s_RendererData;
			cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[data->SwapchainImageIndex]);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			{
				std::scoped_lock lock(g_ImGuiMutex);
				(*data->ImGuiLayer)->Render(cmd);
			}
			cmd->EndGraphics();
#else
			EG_GPU_TIMING_SCOPED(cmd, "Present");

			data->PresentPipeline->SetImageSampler(data->ColorImage, Sampler::PointSampler, 0, 0);
			cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[imageIndex]);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
#endif
		});

		auto& pool = s_RendererData->ThreadPool;
		auto& tasks = s_RendererData->ThreadPoolTasks;
		tasks[s_RendererData->CurrentFrameIndex] = 
			pool->submit([frameIndex = s_RendererData->CurrentFrameIndex]()
		{
			StagingManager::NextFrame();

			auto& fence = s_RendererData->Fences[frameIndex];
			auto& semaphore = s_RendererData->Semaphores[frameIndex];
			auto& imageAcquireSemaphore = s_RendererData->Swapchain->AcquireImage(&s_RendererData->SwapchainImageIndex);
			fence->Reset();

			UpdateGPUTimings();

			auto& cmd = GetCurrentFrameCommandBuffer();
			cmd->Begin();
			{
				EG_CPU_TIMING_SCOPED("Building Command buffer");
				EG_GPU_TIMING_SCOPED(cmd, "Whole frame");
				MaterialSystem::Update(cmd);
				s_CommandQueue[frameIndex].Execute();
			}
			cmd->End();

			{
				EG_CPU_TIMING_SCOPED("Submit & Present");
				s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, imageAcquireSemaphore.get(), 1, semaphore.get(), 1);
				{
#ifdef EG_WITH_EDITOR // TODO: Check if this is needed
					std::scoped_lock lock(g_ImGuiMutex); // Required. Otherwise new ImGui windows will cause crash
#endif
					s_RendererData->Swapchain->Present(semaphore);
				}
			}

			s_RendererData->CurrentRenderingFrameIndex = (s_RendererData->CurrentRenderingFrameIndex + 1) % RendererConfig::FramesInFlight;

			EG_CPU_TIMING_SCOPED("Freeing resources");
			const uint32_t releaseFrameIndex = (s_RendererData->CurrentReleaseFrameIndex + RendererConfig::FramesInFlight) % RendererConfig::ReleaseFramesInFlight;
			s_ResourceFreeQueue[releaseFrameIndex].Execute();
			s_RendererData->CurrentReleaseFrameIndex = (s_RendererData->CurrentReleaseFrameIndex + 1) % RendererConfig::ReleaseFramesInFlight;

			s_RendererData->FrameNumber++;
		});

		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % RendererConfig::FramesInFlight;
	}

	RenderCommandQueue& RenderManager::GetRenderCommandQueue()
	{
		return s_CommandQueue[s_RendererData->CurrentFrameIndex];
	}

	const ThreadPool& RenderManager::GetThreadPool()
	{
		return s_RendererData->ThreadPool;
	}

	void RenderManager::RegisterShaderDependency(const Shader* shader, const Ref<Pipeline>& pipeline)
	{
		s_ShaderDependencies[shader].Pipelines.push_back(pipeline);
	}

	void RenderManager::RemoveShaderDependency(const Shader* shader, const Ref<Pipeline>& pipeline)
	{
		auto it = s_ShaderDependencies.find(shader);
		if (it != s_ShaderDependencies.end())
		{
			auto& pipelines = (*it).second.Pipelines;
			for (auto it = pipelines.begin(); it != pipelines.end(); ++it)
			{
				const Weak<Pipeline>& weakPipeline = *it;
				if (auto sharedPipeline = weakPipeline.lock())
				{
					if (pipeline == sharedPipeline)
					{
						pipelines.erase(it);
						break;
					}
				}
			}
		}
	}

	void RenderManager::OnShaderReloaded(const Shader* shader)
	{
		auto it = s_ShaderDependencies.find(shader);
		if (it != s_ShaderDependencies.end())
		{
			auto& pipelines = it->second.Pipelines;
			for (auto it = pipelines.begin(); it != pipelines.end(); )
			{
				if (auto pipeline = (*it).lock())
				{
					pipeline->Recreate();
					++it;
				}
				else
				{
					it = pipelines.erase(it);
				}
			}
		}
	}

	Ref<CommandBuffer> RenderManager::AllocateCommandBuffer(bool bBegin)
	{
		return s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(bBegin);
	}

	Ref<CommandBuffer> RenderManager::AllocateSecondaryCommandBuffer(bool bBegin)
	{
		return s_RendererData->GraphicsCommandManager->AllocateSecondaryCommandbuffer(bBegin);
	}

	void RenderManager::SubmitCommandBuffer(Ref<CommandBuffer>& cmd, bool bBlock)
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

	RenderCommandQueue& RenderManager::GetResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}

	Ref<CommandBuffer>& RenderManager::GetCurrentFrameCommandBuffer()
	{
		return s_RendererData->CommandBuffers[s_RendererData->CurrentRenderingFrameIndex];
	}

	const RendererCapabilities& RenderManager::GetCapabilities()
	{
		return Application::Get().GetRenderContext()->GetCapabilities();
	}

	uint32_t RenderManager::GetCurrentFrameIndex()
	{
		return s_RendererData->CurrentRenderingFrameIndex;
	}

	uint32_t RenderManager::GetCurrentReleaseFrameIndex()
	{
		return s_RendererData->CurrentReleaseFrameIndex;
	}

	Ref<DescriptorManager>& RenderManager::GetDescriptorSetManager()
	{
		return s_RendererData->DescriptorManager;
	}

	Ref<PipelineGraphics>& RenderManager::GetIBLPipeline()
	{
		return s_RendererData->IBLPipeline;
	}

	Ref<PipelineGraphics>& RenderManager::GetIrradiancePipeline()
	{
		return s_RendererData->IrradiancePipeline;
	}

	Ref<PipelineGraphics>& RenderManager::GetPrefilterPipeline()
	{
		return s_RendererData->PrefilterPipeline;
	}

	Ref<PipelineGraphics>& RenderManager::GetBRDFLUTPipeline()
	{
		return s_RendererData->BRDFLUTPipeline;
	}

	void* RenderManager::GetPresentRenderPassHandle()
	{
		return s_RendererData->PresentPipeline->GetRenderPassHandle();
	}

	uint64_t RenderManager::GetFrameNumber()
	{
		return s_RendererData->FrameNumber;
	}

	GPUTimingsContainer RenderManager::GetTimings()
	{
		GPUTimingsContainer result;
		{
			std::scoped_lock lock(g_TimingsMutex);
			SortGPUTimings();
			result = s_RendererData->GPUTimings;
		}
		return result;
	}

#ifdef EG_GPU_TIMINGS
	void RenderManager::RegisterGPUTiming(Ref<RHIGPUTiming>& timing, std::string_view name)
	{
		s_RendererData->RHIGPUTimings[name] = timing;
	}

	void RenderManager::RegisterGPUTimingParentless(Ref<RHIGPUTiming>& timing, std::string_view name)
	{
		std::scoped_lock lock(g_TimingsMutex);

		if (timing->GetParent() == nullptr)
			s_RendererData->RHIGPUTimingsParentless[name] = timing;
		else
		{
			auto it = s_RendererData->RHIGPUTimingsParentless.find(name);
			if (it != s_RendererData->RHIGPUTimingsParentless.end())
				s_RendererData->RHIGPUTimingsParentless.erase(it);
		}
	}

	const std::unordered_map<std::string_view, Ref<RHIGPUTiming>>& RenderManager::GetRHITimings()
	{
		return s_RendererData->RHIGPUTimings;
	}
#endif
}
