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

#include "Eagle/Utils/BlueNoise.h"
#include "Eagle/Utils/Timer.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Classes/StaticMesh.h"
#include "Eagle/Core/ThreadPool.h"

namespace Eagle
{
	std::mutex g_TimingsMutex;
	static std::mutex s_SubmitMutex;
	static std::mutex s_SubmitFreeMutex;
	static const uint32_t s_VSyncFramesInFlight = 2u;

	struct RendererData
	{
		ThreadPool ThreadPool{"Render Thread", 1};
		std::array<std::future<void>, RendererConfig::FramesInFlight> ThreadPoolTasks;

		Ref<DescriptorManager> DescriptorManager;
		Ref<VulkanSwapchain> Swapchain;

		Ref<Shader> IBLVertex;
		Ref<Shader> IBLFrag;
		Ref<Shader> IBLIrradiance;
		Ref<Shader> IBLPrefilter;

		Ref<PipelineGraphics> PresentPipeline;
		Ref<PipelineGraphics> BRDFLUTPipeline;
		std::vector<Ref<Framebuffer>> PresentFramebuffers;
		Ref<Image> PresentImage;

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
		Ref<Image> DummyRGBA11FImage;
		Ref<Image> DummyRGBA16FImage;
		Ref<Image> DummyRGBA32FImage;
		Ref<Image> DummyDepthImage;
		Ref<Image> DummyCubeDepthImage;
		Ref<Image> BRDFLUTImage;
		Ref<TextureCube> DummyIBL;

#ifdef EG_GPU_TIMINGS
		std::unordered_map<std::string_view, Ref<RHIGPUTiming>> RHIGPUTimings;
		std::unordered_map<std::string_view, Weak<RHIGPUTiming>> RHIGPUTimingsParentless; // Timings that do not have parents
#endif

		Ref<Texture2D> BlueNoise;
		glm::vec2 HaltonSequence[s_JitterSize];

		uint32_t FramesInFlight = RendererConfig::FramesInFlight;
		uint32_t CurrentRenderingFrameIndex = 0;
		uint32_t CurrentFrameIndex = 0;
		uint32_t CurrentReleaseFrameIndex = 0;
		uint64_t FrameNumber = 0;
		uint64_t FrameNumber_CPU = 0;

		void (*PresentFunc)(const Ref<CommandBuffer>&, const Ref<Image>&, uint32_t swapchainImageIndex) = nullptr;
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

	struct PresentPushData
	{
		uint32_t FlipX = 0;
		uint32_t FlipY = 0;
	};

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

		// Intentional copy
		const auto children = timing->GetChildren();
		data.Children.reserve(children.size());
		for (const auto& child : children)
		{
			auto& childData = data.Children.emplace_back();
			childData.Name = child->GetName();
			childData.Timing = child->GetTiming();
		}

		const size_t childsCount = children.size();
		for (size_t i = 0; i < childsCount; ++i)
		{
			const auto& child = children[i];
			for (const auto& childsChild : child->GetChildren())
				data.Children[i].Children.push_back(ProcessTimingChildren(childsChild));
			std::sort(data.Children[i].Children.begin(), data.Children[i].Children.end(), s_CustomGPUTimingsLess);
		}
		std::sort(data.Children.begin(), data.Children.end(), s_CustomGPUTimingsLess);

		return data;
	}

	static GPUTimingsContainer SortGPUTimings()
	{
		GPUTimingsContainer timings;
#ifdef EG_GPU_TIMINGS
		for (auto it = s_RendererData->RHIGPUTimingsParentless.begin(); it != s_RendererData->RHIGPUTimingsParentless.end();)
		{
			if (it->second.expired())
			{
				it = s_RendererData->RHIGPUTimingsParentless.erase(it);
				continue;
			}

			Ref<RHIGPUTiming> timing = it->second.lock();
			auto& lastTiming = timings.emplace_back(ProcessTimingChildren(timing.get()));
			++it;
		}

		std::sort(timings.begin(), timings.end(), s_CustomGPUTimingsLess);
#endif

		return timings;
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
		const auto& swapchainImages = s_RendererData->Swapchain->GetImages();
		const auto& size = swapchainImages[0]->GetSize();

		ColorAttachment colorAttachment;
		colorAttachment.Image = swapchainImages[0];
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageLayoutType::Present;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("present.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("present.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Back;

		s_RendererData->PresentPipeline = PipelineGraphics::Create(state);
		s_RendererData->Semaphores.clear();
		for (const auto& image : swapchainImages)
		{
			s_RendererData->PresentFramebuffers.push_back(Framebuffer::Create({ image }, size, s_RendererData->PresentPipeline->GetRenderPassHandle()));
			s_RendererData->Semaphores.push_back(Semaphore::Create());
		}
	}

	static void SetupBRDFLUTPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->BRDFLUTImage;

		PipelineGraphicsState brdfLutState;
		brdfLutState.VertexShader = Shader::Create("present.vert", ShaderType::Vertex);
		brdfLutState.FragmentShader = Shader::Create("brdf_lut.frag", ShaderType::Fragment);
		brdfLutState.ColorAttachments.push_back(colorAttachment);
		brdfLutState.Size = { RendererConfig::BRDFLUTSize, RendererConfig::BRDFLUTSize };

		s_RendererData->BRDFLUTPipeline = PipelineGraphics::Create(brdfLutState);
	}

	static void InitBlueNoise()
	{
		uint32_t bluenoise[128 * 128]; // rgba8

		for (int y = 0; y < 128; ++y)
		{
			for (int x = 0; x < 128; ++x)
			{
				glm::vec4 noise;
				noise[0] = Utils::BlueNoise(x, y, 0, 0);
				noise[1] = Utils::BlueNoise(x, y, 0, 1);
				noise[2] = Utils::BlueNoise(x, y, 0, 2);
				noise[3] = Utils::BlueNoise(x, y, 0, 3);
				
				glm::uvec4 uNoise = glm::uvec4(noise * 255.f);
				uint32_t packed = uint32_t(uNoise.x) | (uint32_t(uNoise.y) << 8) | (uint32_t(uNoise.z) << 16) | (uint32_t(uNoise.w) << 24);
				bluenoise[x + y * 128] = packed;
			}
		}

		Texture2DSpecifications specs{};
		specs.FilterMode = FilterMode::Point;
		s_RendererData->BlueNoise = Texture2D::Create("Blue Noise", ImageFormat::R8G8B8A8_UNorm, glm::uvec2(128u), bluenoise, specs);
	}

	void RenderManager::Init()
	{
		bImmediateDeletionMode = false;

		Application& app = Application::Get();
		const bool bGame = app.IsGame();
		s_RendererData = new RendererData();
		s_RendererData->Swapchain = app.GetWindow().GetSwapchain();
		s_RendererData->PresentFunc = bGame ? &RenderManager::PresentGame : &RenderManager::PresentEditor;
		s_RendererData->FramesInFlight = s_RendererData->Swapchain->IsVSyncEnabled() ? s_VSyncFramesInFlight : RendererConfig::FramesInFlight;

		InitHaltonSequence();

		s_RendererData->Swapchain->SetOnSwapchainRecreatedCallback([data = s_RendererData]()
		{
			data->PresentFramebuffers.clear();
			const auto& swapchainImages = data->Swapchain->GetImages();
			glm::uvec2 size = s_RendererData->Swapchain->GetSize();
			const void* renderPassHandle = data->PresentPipeline->GetRenderPassHandle();

			data->Semaphores.clear();
			for (const auto& image : swapchainImages)
			{
				data->PresentFramebuffers.push_back(Framebuffer::Create({ image }, size, data->PresentPipeline->GetRenderPassHandle()));
				data->Semaphores.push_back(Semaphore::Create());
			}
		});

		s_RendererData->GraphicsCommandManager = CommandManager::Create(CommandQueueFamily::Graphics, true);
		s_RendererData->CommandBuffers.reserve(RendererConfig::FramesInFlight);
		s_RendererData->Fences.reserve(RendererConfig::FramesInFlight);
		for (uint32_t i = 0; i < RendererConfig::FramesInFlight; ++i)
		{
			s_RendererData->CommandBuffers.push_back(s_RendererData->GraphicsCommandManager->AllocateCommandBuffer(false));
			s_RendererData->Fences.push_back(Fence::Create(true));
		}
		s_RendererData->DescriptorManager = DescriptorManager::Create(DescriptorManager::MaxNumDescriptors, DescriptorManager::MaxSets);

		const uint32_t whitePixel = 0xffffffff;
		const uint32_t blackPixel = 0xff000000;
		const uint32_t half = 0xff7f7f7f;
		const uint32_t red = 0xff0000ff;
		const uint32_t green = 0xff00ff00;
		const uint32_t blue = 0xffff0000;
		Texture2D::WhiteTexture = Texture2D::Create("White", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &whitePixel);
		Texture2D::BlackTexture = Texture2D::Create("Black", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blackPixel);
		Texture2D::GrayTexture = Texture2D::Create("Gray", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &half);
		Texture2D::RedTexture = Texture2D::Create("Red", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &red);
		Texture2D::GreenTexture = Texture2D::Create("Green", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &green);
		Texture2D::BlueTexture = Texture2D::Create("Blue", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blue);
		Texture2D::DummyTexture = Texture2D::Create("None", ImageFormat::R8G8B8A8_UNorm, {1, 1}, &blackPixel);

		if (!bGame)
		{
			Texture2D::NoneIconTexture = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/none.png");
			Texture2D::PointLightIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/pointlight.png");
			Texture2D::DirectionalLightIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/directionallight.png");
			Texture2D::SpotLightIcon = Texture2D::Create(Application::GetCorePath() / "assets/textures/Editor/spotlight.png");
		}

		BufferSpecifications dummyBufferSpecs;
		dummyBufferSpecs.Size = 4;
		dummyBufferSpecs.Usage = BufferUsage::StorageBuffer;
		dummyBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		Buffer::Dummy = Buffer::Create(dummyBufferSpecs, "DummyBuffer");

		Sampler::PointSampler = Sampler::Create(FilterMode::Point, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::PointSamplerClamp = Sampler::Create(FilterMode::Point, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::BilinearSampler = Sampler::Create(FilterMode::Bilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::BilinearSamplerClamp = Sampler::Create(FilterMode::Bilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, 0.f, 1.f);
		Sampler::TrilinearSampler = Sampler::Create(FilterMode::Trilinear, AddressMode::Wrap, CompareOperation::Never, 0.f, 16.f, 1.f);
		Sampler::TrilinearSamplerClamp = Sampler::Create(FilterMode::Trilinear, AddressMode::Clamp, CompareOperation::Never, 0.f, 16.f, 1.f);

		ImageSpecifications colorSpecs;
		colorSpecs.Layout = ImageLayoutType::Unknown;
		colorSpecs.Size = { 1, 1, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;

		colorSpecs.Format = ImageFormat::R11G11B10_Float;
		s_RendererData->DummyRGBA11FImage = Image::Create(colorSpecs, "DummyRGBA11F");
		colorSpecs.Format = ImageFormat::R16G16B16A16_Float;
		s_RendererData->DummyRGBA16FImage = Image::Create(colorSpecs, "DummyRGBA16F");
		colorSpecs.Format = ImageFormat::R32G32B32A32_Float;
		s_RendererData->DummyRGBA32FImage = Image::Create(colorSpecs, "DummyRGBA32F");

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

		s_RendererData->IBLVertex     = Shader::Create("ibl.vert", ShaderType::Vertex);
		s_RendererData->IBLFrag       = Shader::Create("ibl.frag", ShaderType::Fragment);
		s_RendererData->IBLIrradiance = Shader::Create("generate_irradiance.frag", ShaderType::Fragment);
		s_RendererData->IBLPrefilter  = Shader::Create("prefilter_ibl.frag", ShaderType::Fragment);

		MaterialSystem::Init();
		TextureSystem::Init();
		// Init renderer pipelines
		SetupPresentPipeline();
		SetupBRDFLUTPipeline();
		InitBlueNoise();

		s_RendererData->DummyIBL = TextureCube::Create(Texture2D::BlackTexture, 1, 1);

		RenderManager::Submit([](const Ref<CommandBuffer>& cmd)
		{
			cmd->ClearDepthStencilImage(s_RendererData->DummyDepthImage, 0, 0, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->ClearDepthStencilImage(s_RendererData->DummyCubeDepthImage, 0, 0, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->ClearColorImage(s_RendererData->DummyImage, glm::vec4(0), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->ClearColorImage(s_RendererData->DummyImageCube, glm::vec4(0), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->ClearColorImage(s_RendererData->DummyImageR16, glm::vec4(0), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
			cmd->ClearColorImage(s_RendererData->DummyImageR16Cube, glm::vec4(0), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});

		// Render BRDF LUT
		{
			RenderManager::Submit([](const Ref<CommandBuffer>& cmd)
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
		EG_CORE_ASSERT(!IsRenderThread(), "Must be the main thread");

		Wait();

		s_RendererData->ThreadPool->wait();
		s_RendererData->ThreadPool->submit_task([]()
		{
			auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
			fence->Reset();
			auto& cmd = GetCurrentFrameCommandBuffer();
			cmd->Begin();
			for (uint32_t i = 0; i < RendererConfig::FramesInFlight; ++i)
				s_CommandQueue[i].Execute();
			cmd->End();
			s_RendererData->GraphicsCommandManager->Submit(cmd, fence);
			fence->Wait();
		}).wait();

		Wait();
	}

	const Ref<Image>& RenderManager::GetDummyDepthCubeImage()
	{
		return s_RendererData->DummyCubeDepthImage;
	}

	const Ref<Image>& RenderManager::GetDummyDepthImage()
	{
		return s_RendererData->DummyDepthImage;
	}

	const Ref<TextureCube>& RenderManager::GetDummyIBL()
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

	const Ref<Texture2D>& RenderManager::GetBlueNoise()
	{
		return s_RendererData->BlueNoise;
	}

	float RenderManager::GetBlueNoisePhase()
	{
		return (s_RendererData->FrameNumber & 0xFF) * 1.6180339887f;
	}

	void RenderManager::Shutdown()
	{
#ifdef EG_GPU_TIMINGS
		for (auto& [unused, timing] : s_RendererData->RHIGPUTimings)
			timing->SetParent(nullptr);
		s_RendererData->RHIGPUTimings.clear();
#endif
		s_ShaderDependencies.clear();

		StagingManager::ReleaseBuffers();
		ShaderManager::Reset();

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
		Sampler::TrilinearSamplerClamp.reset();
		TextureSystem::Reset();
		MaterialSystem::Shutdown();

		Finish();

		ReleasePendingResources();

		bImmediateDeletionMode = true; // Required to immediately release data during `delete s_RendererData`
		delete s_RendererData;
		s_RendererData = nullptr;
	}

	void RenderManager::Reset()
	{
		Wait();

		TextureSystem::Reset();
		MaterialSystem::Reset();
		StagingManager::ReleaseBuffers();

		Finish();
		ReleasePendingResources();
	}

	void RenderManager::Wait()
	{
		for (auto& task : s_RendererData->ThreadPoolTasks)
		{
			if (task.valid())
			{
				task.wait();
				task = {};
			}
		}
		Application::Get().GetRenderContext()->WaitIdle();
	}

	void RenderManager::ReleasePendingResources()
	{
		for (uint32_t i = 0; i < RendererConfig::ReleaseFramesInFlight; ++i)
			RenderManager::GetResourceReleaseQueue(i).Execute();
	}

	void RenderManager::OnVSyncEnabled(bool bEnabled)
	{
		Wait();
		s_RendererData->CurrentFrameIndex = 0;
		s_RendererData->CurrentReleaseFrameIndex = 0;
		s_RendererData->CurrentRenderingFrameIndex = 0;
		s_RendererData->FramesInFlight = bEnabled ? s_VSyncFramesInFlight : RendererConfig::FramesInFlight;
	}

	void RenderManager::SetPresentImage(const Ref<Image>& image)
	{
		s_RendererData->PresentImage = image;
	}

	void RenderManager::BeginFrame()
	{
		// Waiting for the previous execution to finish
		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		auto& task = s_RendererData->ThreadPoolTasks[s_RendererData->CurrentFrameIndex];
		{
			EG_CPU_TIMING_SCOPED("Waiting for GPU");
			if (task.valid())
			{
				task.wait();
				task = {};
			}
			fence->Wait();
		}

		s_RendererData->ImGuiLayer = &Application::Get().GetImGuiLayer();
	}

	void RenderManager::EndFrame()
	{
		auto& pool = s_RendererData->ThreadPool;
		auto& tasks = s_RendererData->ThreadPoolTasks;
		tasks[s_RendererData->CurrentFrameIndex] = 
			pool->submit_task([frameIndex = s_RendererData->CurrentFrameIndex]()
		{
			EG_CPU_TIMING_SCOPED("Preparing a frame");
			StagingManager::NextFrame();

			uint32_t swapchainImageIndex = 0;
			auto& fence = s_RendererData->Fences[frameIndex];
			Ref<Semaphore> imageAcquireSemaphore = s_RendererData->Swapchain->AcquireImage(frameIndex, &swapchainImageIndex);
			auto& semaphore = s_RendererData->Semaphores[swapchainImageIndex];
			const bool bSwapchainValid = s_RendererData->Swapchain->IsValid() && imageAcquireSemaphore;
			fence->Reset();

			{
				EG_CPU_TIMING_SCOPED("Freeing resources");
				s_ResourceFreeQueue[s_RendererData->CurrentReleaseFrameIndex].Execute();
			}

			UpdateGPUTimings();

			auto& cmd = GetCurrentFrameCommandBuffer();
			cmd->Begin();
			{
				EG_CPU_TIMING_SCOPED("Building Command buffer");
				EG_GPU_TIMING_SCOPED(cmd, "Whole frame");
				MaterialSystem::Update(cmd);
				s_CommandQueue[frameIndex].Execute();
				if (bSwapchainValid)
				{
					s_RendererData->PresentFunc(cmd, s_RendererData->PresentImage, swapchainImageIndex);
				}
			}
			cmd->End();

			{
				EG_CPU_TIMING_SCOPED("Submit & Present");
				if (imageAcquireSemaphore)
				{
					s_RendererData->GraphicsCommandManager->Submit(cmd, fence, imageAcquireSemaphore, semaphore);
				}
				else
				{
					s_RendererData->GraphicsCommandManager->Submit(cmd, fence, std::span<const Semaphore*>{}, semaphore);
				}
				if (bSwapchainValid)
				{
					s_RendererData->Swapchain->Present(semaphore, swapchainImageIndex);
				}
			}

			s_RendererData->CurrentRenderingFrameIndex = (s_RendererData->CurrentRenderingFrameIndex + 1) % s_RendererData->FramesInFlight;
			s_RendererData->CurrentReleaseFrameIndex = (s_RendererData->CurrentReleaseFrameIndex + 1) % s_RendererData->FramesInFlight;
			s_RendererData->FrameNumber++;
		});

		s_RendererData->FrameNumber_CPU++;
		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % s_RendererData->FramesInFlight;
	}

	void RenderManager::PresentEditor(const Ref<CommandBuffer>& cmd, const Ref<Image>& presentImage, uint32_t swapchainImageIndex)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Present+ImGui");

		PresentPushData pushData{0, 0};

		const auto& data = s_RendererData;
		cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[swapchainImageIndex]);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		(*data->ImGuiLayer)->Render(cmd);
		cmd->EndGraphics();
	}

	void RenderManager::PresentGame(const Ref<CommandBuffer>& cmd, const Ref<Image>& presentImage, uint32_t swapchainImageIndex)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Present");
		const auto& data = s_RendererData;

		const ImageLayout layout = presentImage ? presentImage->GetLayout() : ImageLayoutType::Unknown;

		if (presentImage)
		{
			cmd->TransitionLayout(presentImage, layout, ImageReadAccess::PixelShaderRead);
			data->PresentPipeline->SetImageSampler(presentImage, Sampler::PointSampler, 0, 0);
		}
		
		cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[swapchainImageIndex]);

		if (presentImage)
		{
			PresentPushData pushData{ 0, 0 };
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			cmd->Draw(6, 0);
		}

		(*data->ImGuiLayer)->Render(cmd);
		cmd->EndGraphics();

		if (presentImage)
		{
			cmd->TransitionLayout(presentImage, ImageReadAccess::PixelShaderRead, layout);
		}
	}

	std::mutex& RenderManager::GetSubmitMutex()
	{
		return s_SubmitMutex;
	}

	std::mutex& RenderManager::GetSubmitFreeMutex()
	{
		return s_SubmitFreeMutex;
	}

	RenderCommandQueue& RenderManager::GetRenderCommandQueue()
	{
		return s_CommandQueue[s_RendererData->CurrentFrameIndex];
	}

	bool RenderManager::IsRenderThread()
	{
		return std::this_thread::get_id() == s_RendererData->ThreadPool->get_thread_ids()[0];
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

	void RenderManager::SubmitCommandBuffer(const Ref<CommandBuffer>& cmd, bool bBlock)
	{
		auto doSubmit = [&cmd, &bBlock]()
		{
			if (bBlock)
			{
				Ref<Fence> waitFence = Fence::Create();
				s_RendererData->GraphicsCommandManager->Submit(cmd, waitFence);
				waitFence->Wait();
			}
			else
			{
				s_RendererData->GraphicsCommandManager->Submit(cmd);
			}
		};

		if (IsRenderThread())
		{
			doSubmit();
		}
		else
		{
			auto& pool = s_RendererData->ThreadPool;
			pool->submit_task(doSubmit).wait();
		}
	}

	RenderCommandQueue& RenderManager::GetResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}

	const Ref<CommandBuffer>& RenderManager::GetCurrentFrameCommandBuffer()
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

	uint32_t RenderManager::GetCurrentFrameIndex_CPU()
	{
		return s_RendererData->CurrentFrameIndex;
	}

	uint32_t RenderManager::GetCurrentReleaseFrameIndex()
	{
		return s_RendererData->CurrentReleaseFrameIndex;
	}

	const Ref<DescriptorManager>& RenderManager::GetDescriptorSetManager()
	{
		return s_RendererData->DescriptorManager;
	}

	Ref<PipelineGraphics> RenderManager::CreateIBLPipeline(const Ref<Image>& attachment)
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = attachment;

		PipelineGraphicsState state;
		state.VertexShader = s_RendererData->IBLVertex;
		state.FragmentShader = Shader::Create("ibl.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.Size = { TextureCube::SkyboxSize, TextureCube::SkyboxSize };
		state.bImagelessFramebuffer = true;

		return PipelineGraphics::Create(state);
	}

	Ref<PipelineGraphics> RenderManager::CreateIrradiancePipeline(const Ref<Image>& attachment)
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = attachment;

		PipelineGraphicsState irradianceState;
		irradianceState.VertexShader = s_RendererData->IBLVertex;
		irradianceState.FragmentShader = Shader::Create("generate_irradiance.frag", ShaderType::Fragment);
		irradianceState.ColorAttachments.push_back(colorAttachment);
		irradianceState.Size = { TextureCube::IrradianceSize, TextureCube::IrradianceSize };
		irradianceState.bImagelessFramebuffer = true;

		return PipelineGraphics::Create(irradianceState);
	}

	Ref<PipelineGraphics> RenderManager::CreatePrefilterPipeline(const Ref<Image>& attachment)
	{
		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Clear;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = attachment;

		PipelineGraphicsState prefilterState;
		prefilterState.VertexShader = s_RendererData->IBLVertex;
		prefilterState.FragmentShader = Shader::Create("prefilter_ibl.frag", ShaderType::Fragment);
		prefilterState.ColorAttachments.push_back(colorAttachment);
		prefilterState.Size = { 1, 1 };
		prefilterState.bImagelessFramebuffer = true;

		return PipelineGraphics::Create(prefilterState);
	}

	const Ref<PipelineGraphics>& RenderManager::GetBRDFLUTPipeline()
	{
		return s_RendererData->BRDFLUTPipeline;
	}

	void* RenderManager::GetPresentRenderPassHandle()
	{
		return s_RendererData->PresentPipeline->GetRenderPassHandle();
	}

	uint64_t RenderManager::GetFrameNumber_CPU()
	{
		return s_RendererData->FrameNumber_CPU;
	}

	uint64_t RenderManager::GetFrameNumber_RT()
	{
		return s_RendererData->FrameNumber;
	}

	GPUTimingsContainer RenderManager::GetTimings()
	{
		GPUTimingsContainer result;
		{
			std::scoped_lock lock(g_TimingsMutex);
			result = SortGPUTimings();
		}
		return result;
	}

#ifdef EG_GPU_TIMINGS
	void RenderManager::RegisterGPUTiming(const Ref<RHIGPUTiming>& timing, std::string_view name)
	{
		std::scoped_lock lock(g_TimingsMutex);
		s_RendererData->RHIGPUTimings[name] = timing;
	}

	void RenderManager::RegisterGPUTimingParentless(const Ref<RHIGPUTiming>& timing, std::string_view name)
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

	void Utils::DumpGPUInfo()
	{
		auto& caps = RenderManager::GetCapabilities();
		EG_CORE_TRACE("GPU Info:");
		EG_CORE_TRACE("  Vendor: {0}", caps.Vendor);
		EG_CORE_TRACE("  Device: {0}", caps.Device);
		EG_CORE_TRACE("  Driver Version: {0}", caps.DriverVersion);
		EG_CORE_TRACE("  API Version: {0}", caps.ApiVersion);
		EG_CORE_TRACE("  Max Anisotropy: {0}", caps.MaxAnisotropy);
		EG_CORE_TRACE("  Texture Compression support");
		EG_CORE_TRACE("    BC: {}", caps.bTextureCompressionBC);
		EG_CORE_TRACE("    ETC: {}", caps.bTextureCompressionETC2);
		EG_CORE_TRACE("    ASTC LDR: {}", caps.bTextureCompressionASTC_LDR);
	}
}
