#pragma once

#include "RendererContext.h"
#include "VidWrappers/DescriptorManager.h"

#include "RenderCommandQueue.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Core/ThreadPool.h"

#include "Eagle/Debug/GPUTimings.h"

struct DirectionalLight;
struct PointLight;
struct SpotLight;

namespace Eagle
{
	class CameraComponent;
	class EditorCamera;
	class PointLightComponent;
	class DirectionalLightComponent;
	class SpotLightComponent;
	class Material;
	class StaticMeshComponent;
	class StaticMesh;
	class Framebuffer;
	class SpriteComponent;
	class Shader;
	class Pipeline;
	class PipelineGraphics;
	class CommandBuffer;
	class Image;
	class Sampler;
	class Texture;
	class Texture2D;
	class TextureCube;
	struct Transform;

	struct GPUTimingData
	{
		std::string_view Name;
		float Timing;
		std::vector<GPUTimingData> Children;
	};

	using GPUTimingsContainer = std::vector<GPUTimingData>;

	class RenderManager
	{
	public:
		static RendererAPIType GetAPI() { return RendererContext::Current(); }

		static Ref<RendererContext>& GetContext()
		{
			return Application::Get().GetRenderContext();
		}

		static void Init();
		static void Finish();
		static void Shutdown();
		static void Wait();
		static void ReleasePendingResources();

		static void BeginFrame();
		static void EndFrame();

		[[nodiscard]] static Ref<CommandBuffer> AllocateCommandBuffer(bool bBegin);
		[[nodiscard]] static Ref<CommandBuffer> AllocateSecondaryCommandBuffer(bool bBegin);
		static void SubmitCommandBuffer(Ref<CommandBuffer>& cmd, bool bBlock);

		static Ref<Image>& GetDummyDepthCubeImage();
		static Ref<Image>& GetDummyDepthImage();
		static Ref<TextureCube>& GetDummyIBL();
		static const Ref<Image>& GetBRDFLUTImage();
		static const Ref<Image>& GetDummyImage3D();

		static GPUTimingsContainer GetTimings();
#ifdef EG_GPU_TIMINGS
		static void RegisterGPUTiming(Ref<RHIGPUTiming>& timing, std::string_view name);
		static void RegisterGPUTimingParentless(Ref<RHIGPUTiming>& timing, std::string_view name);
		static const std::unordered_map<std::string_view, Ref<RHIGPUTiming>>& GetRHITimings();
#endif

		template<typename FuncT>
		static void Submit(FuncT&& func)
		{
			// Shouldn't call Submit from inside the render thread
			EG_ASSERT(std::this_thread::get_id() != GetThreadPool()->get_threads()[0].get_id());

			auto renderCmd = [](void* ptr)
			{
				auto f = (FuncT*)ptr;
				(*f)(GetCurrentFrameCommandBuffer());
				f->~FuncT();
			};
			auto mem = GetRenderCommandQueue().Allocate(renderCmd, sizeof(func));
			new(mem) FuncT(std::forward<FuncT>(func));
		}

		template<typename FuncT>
		static void SubmitResourceFree(FuncT&& func)
		{
			auto renderCmd = [](void* ptr)
			{
				auto f = (FuncT*)ptr;
				(*f)();
				f->~FuncT();
			};

			const uint32_t frameIndex = RenderManager::GetCurrentReleaseFrameIndex();
			auto mem = GetResourceReleaseQueue(frameIndex).Allocate(renderCmd, sizeof(func));
			new(mem) FuncT(std::forward<FuncT>(func));
		}

		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void OnShaderReloaded(const Ref<Shader>& shader);

		static RenderCommandQueue& GetResourceReleaseQueue(uint32_t index);

		static const RendererCapabilities& GetCapabilities();
		static uint32_t GetCurrentFrameIndex();
		static uint32_t GetCurrentReleaseFrameIndex();
		static Ref<DescriptorManager>& GetDescriptorSetManager();
		static Ref<PipelineGraphics>& GetIBLPipeline();
		static Ref<PipelineGraphics>& GetIrradiancePipeline();
		static Ref<PipelineGraphics>& GetPrefilterPipeline();
		static Ref<PipelineGraphics>& GetBRDFLUTPipeline();
		static void* GetPresentRenderPassHandle();
		static uint64_t GetFrameNumber();

	private:
		static Ref<CommandBuffer>& GetCurrentFrameCommandBuffer();
		static RenderCommandQueue& GetRenderCommandQueue();
		static const ThreadPool& GetThreadPool();

	public:
		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Vertices = 0;
			uint32_t Indeces = 0;
			float RenderingTook = 0.f; //ms
		};

		struct Statistics2D
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			inline uint32_t GetVertexCount() const { return QuadCount * 4; }
			inline uint32_t GetIndexCount() const { return QuadCount * 6; }
		};

		static void ResetStats();
		static Statistics GetStats();
		static Statistics2D GetStats2D();
	};

	namespace Utils
	{
		inline void DumpGPUInfo()
		{
			auto& caps = RenderManager::GetCapabilities();
			EG_CORE_TRACE("GPU Info:");
			EG_CORE_TRACE("  Vendor: {0}", caps.Vendor);
			EG_CORE_TRACE("  Device: {0}", caps.Device);
			EG_CORE_TRACE("  Driver Version: {0}", caps.DriverVersion);
			EG_CORE_TRACE("  API Version: {0}", caps.ApiVersion);
		}
	}
}