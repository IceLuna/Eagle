#include "egpch.h"

#include "RenderManager.h"
#include "TextureSystem.h"

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

		Ref<Image> DummyImage3D;
		Ref<Image> DummyRGBA16FImage;
		Ref<Image> DummyDepthImage;
		Ref<Image> DummyCubeDepthImage;
		Ref<Image> BRDFLUTImage;
		Ref<TextureCube> DummyIBL;
		Ref<TextureCube> IBLTexture;

		RenderManager::Statistics Stats[RendererConfig::FramesInFlight];

		GPUTimingsMap GPUTimings; // Sorted
#ifdef EG_GPU_TIMINGS
		std::unordered_map<std::string_view, Ref<RHIGPUTiming>> RHIGPUTimings;
#endif

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

	static Ref<Image> CreateDepthImage(glm::uvec3 size, std::string_view debugName, bool bCube)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
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

		uint32_t whitePixel = 0xffffffff;
		uint32_t blackPixel = 0xff000000;
		Texture2D::WhiteTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &whitePixel, {}, false);
		Texture2D::WhiteTexture->m_Path = "White";
		Texture2D::BlackTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &blackPixel, {}, false);
		Texture2D::BlackTexture->m_Path = "Black";
		Texture2D::DummyTexture = Texture2D::Create(ImageFormat::R8G8B8A8_UNorm, { 1, 1 }, &blackPixel, {}, false);
		Texture2D::DummyTexture->m_Path = "None";
		Texture2D::NoneIconTexture = Texture2D::Create("assets/textures/Editor/none.png", {}, false);
		Texture2D::PointLightIcon = Texture2D::Create("assets/textures/Editor/pointlight.png", {}, false);
		Texture2D::DirectionalLightIcon = Texture2D::Create("assets/textures/Editor/directionallight.png", {}, false);
		Texture2D::SpotLightIcon = Texture2D::Create("assets/textures/Editor/spotlight.png", {}, false);

		BufferSpecifications dummyBufferSpecs;
		dummyBufferSpecs.Size = 4;
		dummyBufferSpecs.Usage = BufferUsage::StorageBuffer;
		Buffer::Dummy = Buffer::Create(dummyBufferSpecs, "DummyBuffer");

		Sampler::PointSampler = Sampler::Create(FilterMode::Point, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::PointSamplerClamp = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::BilinearSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::TrilinearSampler = Sampler::Create(FilterMode::Trilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R16G16B16A16_Float;
		colorSpecs.Layout = ImageLayoutType::Unknown;
		colorSpecs.Size = { 1, 1, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->DummyRGBA16FImage = Image::Create(colorSpecs, "DummyRGBA16F");

		colorSpecs.Size = { RendererConfig::BRDFLUTSize, RendererConfig::BRDFLUTSize, 1 };
		colorSpecs.Format = ImageFormat::R16G16_Float;
		s_RendererData->BRDFLUTImage = Image::Create(colorSpecs, "BRDFLUT_Image");

		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Type = ImageType::Type3D;
		s_RendererData->DummyImage3D = Image::Create(colorSpecs, "Dummy3D");

		s_RendererData->DummyCubeDepthImage = CreateDepthImage(glm::uvec3{ 1, 1, 1 }, "DummyDepthImage_Cube", true);
		s_RendererData->DummyDepthImage = CreateDepthImage(glm::uvec3{ 1, 1, 1 }, "DummyDepthImage", false);

		TextureSystem::Init();
		// Init renderer pipelines
		SetupPresentPipeline();
		SetupIBLPipeline();
		SetupBRDFLUTPipeline();

		s_RendererData->DummyIBL = TextureCube::Create(Texture2D::BlackTexture, 1, false);

		RenderManager::Submit([](Ref<CommandBuffer>& cmd)
		{
			cmd->TransitionLayout(s_RendererData->DummyCubeDepthImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(s_RendererData->DummyDepthImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
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

	const Ref<Image>& RenderManager::GetDummyImage3D()
	{
		return s_RendererData->DummyImage3D;
	}

	void RenderManager::Shutdown()
	{
#ifdef EG_GPU_TIMINGS
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
		Texture2D::NoneIconTexture.reset();
		Texture2D::PointLightIcon.reset();
		Texture2D::DirectionalLightIcon.reset();
		Texture2D::SpotLightIcon.reset();
		Buffer::Dummy.reset();

		Sampler::PointSampler.reset();
		Sampler::PointSamplerClamp.reset();
		Sampler::BilinearSampler.reset();
		Sampler::TrilinearSampler.reset();
		TextureSystem::Shutdown();

		Finish();

		delete s_RendererData;
		s_RendererData = nullptr;

		for (uint32_t i = 0; i < RendererConfig::ReleaseFramesInFlight; ++i)
			RenderManager::GetResourceReleaseQueue(i).Execute();
	}

	void RenderManager::Wait()
	{
		for (auto& task : s_RendererData->ThreadPoolTasks)
			if (task.valid())
				task.wait();
		Application::Get().GetRenderContext()->WaitIdle();
	}

	void RenderManager::BeginFrame()
	{
		{
			auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
			const auto& task = s_RendererData->ThreadPoolTasks[s_RendererData->CurrentFrameIndex];
			{
				EG_CPU_TIMING_SCOPED("Waiting For GPU");
				if (task.valid())
					task.wait();
				fence->Wait();
			}
		}

#ifdef EG_GPU_TIMINGS
		Submit([data = s_RendererData](Ref<CommandBuffer>&)
		{
			std::scoped_lock lock(g_TimingsMutex);
			data->GPUTimings.clear();
			for (auto it = data->RHIGPUTimings.begin(); it != data->RHIGPUTimings.end();)
			{
				// If it wasn't used in prev frame, erase it
				if (it->second->bIsUsed == false)
					it = data->RHIGPUTimings.erase(it);
				else
				{
					it->second->QueryTiming(data->CurrentFrameIndex);
					data->GPUTimings.push_back({ it->first, it->second->GetTiming() });
					it->second->bIsUsed = false; // Set to false for the current frame
					++it;
				}
			}
			std::sort(data->GPUTimings.begin(), data->GPUTimings.end(), s_CustomGPUTimingsLess);
		});
#endif
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
			pool->submit([frameIndex = s_RendererData->CurrentFrameIndex, currentReleaseFrameIndex = s_RendererData->CurrentReleaseFrameIndex]()
		{
			StagingManager::NextFrame();

			auto& fence = s_RendererData->Fences[frameIndex];
			auto& semaphore = s_RendererData->Semaphores[frameIndex];
			auto& imageAcquireSemaphore = s_RendererData->Swapchain->AcquireImage(&s_RendererData->SwapchainImageIndex);
			fence->Reset();

			auto& cmd = GetCurrentFrameCommandBuffer();
			cmd->Begin();
			{
				EG_CPU_TIMING_SCOPED("Building Command buffer");
				EG_GPU_TIMING_SCOPED(cmd, "Whole frame");
				s_CommandQueue[frameIndex].Execute();
			}
			cmd->End();

			{
				EG_CPU_TIMING_SCOPED("Submit & Present");
				s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, imageAcquireSemaphore.get(), 1, semaphore.get(), 1);
				{
#ifdef EG_WITH_EDITOR
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
		});

		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % RendererConfig::FramesInFlight;
		s_RendererData->FrameNumber++;

		// Reset stats of the next frame
		RenderManager::ResetStats();
	}

	RenderCommandQueue& RenderManager::GetRenderCommandQueue()
	{
		return s_CommandQueue[s_RendererData->CurrentFrameIndex];
	}

	const ThreadPool& RenderManager::GetThreadPool()
	{
		return s_RendererData->ThreadPool;
	}

	void RenderManager::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		const Shader* raw = shader.get();
		s_ShaderDependencies[raw].Pipelines.push_back(pipeline);
	}

	void RenderManager::RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		const Shader* raw = shader.get();
		auto it = s_ShaderDependencies.find(raw);
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

	void RenderManager::OnShaderReloaded(const Ref<Shader>& shader)
	{
		const Shader* raw = shader.get();

		auto it = s_ShaderDependencies.find(raw);
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

	void RenderManager::ResetStats()
	{
		memset(&s_RendererData->Stats[s_RendererData->CurrentFrameIndex], 0, sizeof(RenderManager::Statistics));
	}

	GPUTimingsMap RenderManager::GetTimings()
	{
		std::scoped_lock lock(g_TimingsMutex);
		auto temp = s_RendererData->GPUTimings;
		return temp;
	}

#ifdef EG_GPU_TIMINGS
	void RenderManager::RegisterGPUTiming(Ref<RHIGPUTiming>& timing, std::string_view name)
	{
		s_RendererData->RHIGPUTimings[name] = timing;
	}

	const std::unordered_map<std::string_view, Ref<RHIGPUTiming>>& RenderManager::GetRHITimings()
	{
		return s_RendererData->RHIGPUTimings;
	}
#endif

	RenderManager::Statistics RenderManager::GetStats()
	{
		uint32_t index = s_RendererData->CurrentFrameIndex;
		index = index == 0 ? RendererConfig::FramesInFlight - 2 : index - 1;

		return s_RendererData->Stats[index]; // Returns stats of the prev frame because current frame stats are not ready yet
	}

	RenderManager::Statistics2D RenderManager::GetStats2D()
	{
		return RenderManager::Statistics2D{};
	}
}
