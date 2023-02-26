#pragma once

#include "RendererContext.h"
#include "DescriptorManager.h"

#include "RenderCommandQueue.h"
#include "Eagle/Core/Application.h"

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

	struct RendererConfig
	{
		static constexpr uint32_t FramesInFlight = 3;
		static constexpr uint32_t ReleaseFramesInFlight = FramesInFlight * 2; // For releasing resources
		static constexpr uint32_t BRDFLUTSize = 512;
		static constexpr uint32_t DirLightShadowMapSize = 2048;
		static constexpr glm::uvec3 PointLightSMSize = glm::uvec3(2048, 2048, 1);
		static constexpr glm::uvec3 SpotLightSMSize = glm::uvec3(2048, 2048, 1);
	};

	struct GBuffers
	{
		Ref<Image> Albedo;
		Ref<Image> MaterialData; // R: Metallness; G: Roughness; B: AO; A: unused
		Ref<Image> GeometryNormal;
		Ref<Image> ShadingNormal;
		Ref<Image> ObjectID;
		Ref<Image> Depth;

		void Resize(const glm::uvec3& size)
		{
			Albedo->Resize(size);
			MaterialData->Resize(size);
			GeometryNormal->Resize(size);
			ShadingNormal->Resize(size);
			ObjectID->Resize(size);
			Depth->Resize(size);
		}
		void Init(const glm::uvec3& size);
	};

	struct GPUTimingData
	{
		std::string_view Name;
		float Timing;
	};

	using GPUTimingsMap = std::vector<GPUTimingData>;

	class Renderer
	{
	public:
		static RendererAPIType GetAPI() { return RendererContext::Current(); }

		static void BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLight, const std::vector<SpotLightComponent*>& spotLights);
		static void BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLight, const std::vector<SpotLightComponent*>& spotLights);
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
		static uint64_t UsedTextureChangedFrame();
		static size_t GetTextureIndex(const Ref<Texture>& texture);
		static const std::vector<Ref<Image>>& GetUsedImages();
		static const std::vector<Ref<Sampler>>& GetUsedSamplers();
		static const Ref<Buffer>& GetMaterialsBuffer();
		static const std::vector<Ref<Framebuffer>>& GetCSMFramebuffers();
		static const DirectionalLight& GetDirectionalLight(bool* hasDirLight);
		static const std::vector<PointLight>& GetPointLights();
		static Ref<Buffer>& GetPointLightsVPBuffer();
		static const std::vector<Ref<Image>>& GetPointLightsShadowMaps();
		static Ref<Image>& GetDummyDepthCubeImage();
		static const std::vector<SpotLight>& GetSpotLights();
		static const std::vector<Ref<Image>>& GetSpotLightsShadowMaps();
		static Ref<Image>& GetDummyDepthImage();

		static const GPUTimingsMap& GetTimings();
#ifdef EG_GPU_TIMINGS
		static void RegisterGPUTiming(Ref<RHIGPUTiming>& timing, std::string_view name);
		static const std::unordered_map<std::string_view, Ref<RHIGPUTiming>>& GetRHITimings();
#endif

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

			const uint32_t frameIndex = Renderer::GetCurrentReleaseFrameIndex();
			auto mem = GetResourceReleaseQueue(frameIndex).Allocate(renderCmd, sizeof(func));
			new(mem) FuncT(std::forward<FuncT>(func));
		}

		static void RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline);
		static void OnShaderReloaded(const Ref<Shader>& shader);

		static void DrawMesh(const StaticMeshComponent& smComponent);
		static void DrawSprite(const SpriteComponent& sprite);
		static void DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
		static void DrawSkybox(const Ref<TextureCube>& cubemap);
		static void DrawBillboard(const Transform& transform, const Ref<Texture2D>& texture);

		static void WindowResized(uint32_t width, uint32_t height);

		static float GetGamma();
		static void SetGamma(float gamma);
		static float& Exposure();
		static TonemappingMethod& TonemappingMethod();
		static void SetPhotoLinearTonemappingParams(const PhotoLinearTonemappingParams& params);
		static FilmicTonemappingParams& FilmicTonemappingParams();

		static bool IsVisualizingCascades();
		static void SetVisualizeCascades(bool bVisualize);
		static bool IsSoftShadowsEnabled();
		static void SetSoftShadowsEnabled(bool bEnable);

		static Ref<Image>& GetFinalImage();
		static GBuffers& GetGBuffers();

		static RenderCommandQueue& GetResourceReleaseQueue(uint32_t index);
		static Ref<CommandBuffer>& GetCurrentFrameCommandBuffer();

		static const RendererCapabilities& GetCapabilities();
		static uint32_t GetCurrentFrameIndex();
		static uint32_t GetCurrentReleaseFrameIndex();
		static Ref<DescriptorManager>& GetDescriptorSetManager();
		static const Ref<PipelineGraphics>& GetMeshPipeline();
		static Ref<PipelineGraphics>& GetIBLPipeline();
		static Ref<PipelineGraphics>& GetIrradiancePipeline();
		static Ref<PipelineGraphics>& GetPrefilterPipeline();
		static Ref<PipelineGraphics>& GetBRDFLUTPipeline();
		static void* GetPresentRenderPassHandle();
		static uint64_t GetFrameNumber();

		// For internal usage
		static size_t AddTexture(const Ref<Texture>& texture);

	private:
		static RenderCommandQueue& GetRenderCommandQueue();
		static void UpdateMaterials();
		static void CreateBuffers();

		static void RenderMeshes();
		static void RenderSprites();
		static void RenderBillboards();
		static void PBRPass();
		static void SkyboxPass();
		static void PostprocessingPass();
		static void RenderDebugLines();

		static void UpdateLightsBuffers(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLight, const std::vector<SpotLightComponent*>& spotLights);

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