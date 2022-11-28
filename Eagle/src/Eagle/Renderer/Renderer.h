#pragma once

#include "RendererAPI.h"
#include "RendererContext.h"
#include "DescriptorManager.h"

#include "RenderCommandQueue.h"
#include "Eagle/Core/Application.h"

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
	class Cubemap;
	class Framebuffer;
	class SpriteComponent;
	class Shader;
	class Pipeline;
	class PipelineGraphics;
	class CommandBuffer;
	class Image;
	class Sampler;
	class Texture;
	struct Transform;
	struct SMData;
	struct SpriteData;

	struct RendererConfig
	{
		uint32_t FramesInFlight = 3;
	};

	class Renderer
	{
	public:
		static RendererAPIType GetAPI() { return RendererAPI::Current(); }

		static void BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void EndScene();

		static Ref<RendererContext>& GetContext()
		{
			return Application::Get().GetRenderContext();
		}

		static void Init();
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();

		static Ref<CommandBuffer> AllocateCommandBuffer(bool bBegin);
		static void SubmitCommandBuffer(Ref<CommandBuffer>& cmd, bool bBlock);

		static bool UsedTextureChanged();
		static size_t GetTextureIndex(const Ref<Texture>& texture);
		static const std::vector<Ref<Image>>& GetUsedImages();
		static const std::vector<Ref<Sampler>>& GetUsedSamplers();
		static const Ref<Buffer>& GetMaterialsBuffer();

		template<typename FuncT>
		static void Submit(FuncT&& func)
		{
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

			const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			auto mem = GetResourceReleaseQueue(frameIndex).Allocate(renderCmd, sizeof(func));
			new(mem) FuncT(std::forward<FuncT>(func));
		}

		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void OnShaderReloaded(size_t hash);

		static void DrawMesh(const StaticMeshComponent& smComponent);
		static void DrawSprite(const SpriteComponent& sprite);
		static void DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);

		static void WindowResized(uint32_t width, uint32_t height);

		static float& Gamma();
		static float& Exposure();
		static Ref<Image>& GetFinalImage();
		static Ref<Image>& GetNormalsImage();
		static Ref<Framebuffer>& GetGFramebuffer();		// TODO: do

		static RenderCommandQueue& GetResourceReleaseQueue(uint32_t index);
		static Ref<CommandBuffer>& GetCurrentFrameCommandBuffer();

		static constexpr RendererConfig GetConfig() { return {}; }
		static RendererCapabilities& GetCapabilities();
		static uint32_t GetCurrentFrameIndex();
		static Ref<DescriptorManager>& GetDescriptorSetManager();
		static const Ref<PipelineGraphics>& GetMeshPipeline();
		static void* GetPresentRenderPassHandle();
		static uint64_t GetFrameNumber();

	private:
		static RenderCommandQueue& GetRenderCommandQueue();
		static void RenderMeshes();
		static void RenderSprites();
		static void UpdateLightsBuffers(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent& directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void PBRPass();

	public:
		//Stats
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t Vertices = 0;
			uint32_t Indeces = 0;
			float RenderingTook = 0.f; //ms
		};

		static void ResetStats();
		static Statistics GetStats();
	};

	namespace Utils
	{
		inline void DumpGPUInfo()
		{
			auto& caps = Renderer::GetCapabilities();
			EG_CORE_TRACE("GPU Info:");
			EG_CORE_TRACE("  Vendor: {0}", caps.Vendor);
			EG_CORE_TRACE("  Device: {0}", caps.Device);
			EG_CORE_TRACE("  Version: {0}", caps.Version);
		}
	}
}