#pragma once

#include "RendererAPI.h"
#include "RendererContext.h"
#include "DescriptorManager.h"

#include "RenderCommandQueue.h"
#include "Eagle/Core/Application.h"

#define MAXPOINTLIGHTS 4
#define MAXSPOTLIGHTS 4

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
	class CommandBuffer;
	struct Transform;
	struct SMData;

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

		static void DrawMesh(const StaticMeshComponent& smComponent, int entityID);
		static void DrawMesh(const Ref<StaticMesh>& staticMesh, const Transform& worldTransform, int entityID);
		static void DrawSprite(const SpriteComponent& sprite, int entityID = -1);
		static void DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);

		static void WindowResized(uint32_t width, uint32_t height);

		static float& Gamma();
		static float& Exposure();
		static Ref<Image>& GetFinalImage();
		static Ref<Framebuffer>& GetGFramebuffer();		// TODO: do

		static RenderCommandQueue& GetResourceReleaseQueue(uint32_t index);
		static Ref<CommandBuffer>& GetCurrentFrameCommandBuffer();

		static RendererConfig& GetConfig();
		static RendererCapabilities& GetCapabilities();
		static uint32_t GetCurrentFrameIndex();
		static Ref<DescriptorManager>& GetDescriptorSetManager();
		static void* GetPresentRenderPassHandle();

	private:
		static RenderCommandQueue& GetRenderCommandQueue();
		static void PrepareRendering();
		static void RenderMeshes(Ref<CommandBuffer>& cmd, const std::vector<SMData>& meshes);

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