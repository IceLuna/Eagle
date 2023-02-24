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
#include "Eagle/Debug/CPUTimings.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

//TODO: remove this dependency
#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct BillboardVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec2 TexCoord = glm::vec2{ 0.f };
		uint32_t TextureIndex = 0;
		int EntityID = -1;
	};

	struct LineData
	{
		glm::vec3 Start = glm::vec3{0.f};
		glm::vec3 End = glm::vec3{0.f};
		glm::vec4 Color = glm::vec4{0.f, 0.f, 0.f, 1.f};
	};

	struct MiscellaneousData
	{
		Transform Transform;
		const Ref<Texture2D>* Texture = nullptr;
	};

	struct BillboardData
	{
		std::vector<BillboardVertex> Vertices;
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		Ref<Shader> VertexShader;
		Ref<Shader> FragmentShader;
		Ref<PipelineGraphics> Pipeline;
		uint64_t TexturesUpdatedFrame = 0;

		static constexpr size_t DefaultBillboardQuadCount = 10; // How much quads we can render without reallocating
		static constexpr size_t DefaultBillboardVerticesCount = DefaultBillboardQuadCount * 4;

		static constexpr size_t BaseBillboardVertexBufferSize = DefaultBillboardVerticesCount * sizeof(BillboardVertex); //Allocating enough space to store 40 vertices
		static constexpr size_t BaseBillboardIndexBufferSize = DefaultBillboardQuadCount * (sizeof(Index) * 6);
	};

	struct RendererData
	{
		Ref<DescriptorManager> DescriptorManager;
		Ref<VulkanSwapchain> Swapchain;

		std::vector<MiscellaneousData> Miscellaneous;

		Ref<PipelineGraphics> PresentPipeline;
		Ref<PipelineGraphics> MeshPipeline;
		Ref<PipelineGraphics> PBRPipeline;
		Ref<PipelineGraphics> SkyboxPipeline;
		Ref<PipelineGraphics> IBLPipeline;
		Ref<PipelineGraphics> IrradiancePipeline;
		Ref<PipelineGraphics> PrefilterPipeline;
		Ref<PipelineGraphics> BRDFLUTPipeline;
		Ref<PipelineGraphics> ShadowMapPipeline;
		Ref<PipelineGraphics> PostProcessingPipeline;
		Ref<Shader> PBRFragShader;
		ShaderDefines PBRDefines;
		std::vector<Ref<Framebuffer>> PresentFramebuffers;

		std::vector<Ref<Image>> DirectionalLightShadowMaps = std::vector<Ref<Image>>(EG_CASCADES_COUNT);
		std::vector<Ref<Sampler>> DirectionalLightShadowMapSamplers = std::vector<Ref<Sampler>>(EG_CASCADES_COUNT);
		std::vector<Ref<Framebuffer>> ShadowMapFramebuffers = std::vector<Ref<Framebuffer>>(EG_CASCADES_COUNT);

		Ref<PipelineGraphics> PointLightSMPipeline;
		std::vector<Ref<Framebuffer>> PointLightSMFramebuffers;
		std::vector<Ref<Image>> PointLightShadowMaps = std::vector<Ref<Image>>(EG_MAX_POINT_LIGHT_SHADOW_MAPS);
		std::vector<Ref<Sampler>> PointLightShadowMapSamplers = std::vector<Ref<Sampler>>(EG_MAX_POINT_LIGHT_SHADOW_MAPS);

		GBuffers GBufferImages;
		Ref<Image> ColorImage;
		Ref<Image> FinalImage;
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;

		Ref<Buffer> AdditionalMeshDataBuffer;
		Ref<Buffer> CameraViewDataBuffer;
		Ref<Buffer> PointLightsBuffer;
		Ref<Buffer> SpotLightsBuffer;
		Ref<Buffer> DirectionalLightBuffer;
		Ref<Buffer> MaterialBuffer;
		Ref<Buffer> PointLightsVPsBuffer;

		BillboardData BillboardData;

		std::vector<CPUMaterial> ShaderMaterials;
		std::vector<PointLight> PointLights;
		std::vector<SpotLight> SpotLights;
		DirectionalLight DirectionalLight;
		bool HasDirectionalLight = false;

		Ref<ImGuiLayer>* ImGuiLayer = nullptr; // Pointer is used just to avoid incrementing counter

		Ref<CommandManager> GraphicsCommandManager;
		std::vector<Ref<CommandBuffer>> CommandBuffers;
		std::vector<Ref<Fence>> Fences;
		std::vector<Ref<Semaphore>> Semaphores;

		glm::mat4 CurrentFrameView     = glm::mat4(1.f);
		glm::mat4 CurrentFrameProj     = glm::mat4(1.f);
		glm::mat4 CurrentFrameViewProj = glm::mat4(1.f);
		glm::mat4 CurrentFrameInvViewProj = glm::mat4(1.f);
		glm::mat4 OrthoProjection = glm::mat4(1.f);
		glm::vec3 ViewPos = glm::vec3{0.f};
		const Camera* Camera = nullptr;
		
		std::vector<const StaticMeshComponent*> Meshes;
		std::vector<const SpriteComponent*> Sprites;
		std::vector<LineData> Lines;

		Ref<Image> DummyRGBA16FImage;
		Ref<Image> DummyCubeDepthImage;
		Ref<Image> BRDFLUTImage;
		Ref<TextureCube> DummyIBL;
		Ref<TextureCube> IBLTexture;
		Ref<Image> ShadowMapDistribution;

		// Used by the renderer. Stores all textures required for rendering
		std::unordered_map<Ref<Texture>, size_t> UsedTexturesMap; // size_t = index to vector<Ref<Image>>
		std::vector<Ref<Image>> Images;
		std::vector<Ref<Sampler>> Samplers;
		size_t CurrentTextureIndex = 1; // 0 - DummyTexture
		uint64_t TextureMapChangedFrame = 0;
		bool bTextureMapChanged = true;

		Renderer::Statistics Stats[RendererConfig::FramesInFlight];

		GPUTimingsMap GPUTimings; // Sorted
#ifdef EG_GPU_TIMINGS
		std::unordered_map<std::string_view, Ref<RHIGPUTiming>> RHIGPUTimings;
#endif

		float Gamma = 2.2f;
		float Exposure = 1.f;
		TonemappingMethod TonemappingMethod = TonemappingMethod::Reinhard;
		PhotoLinearTonemappingParams PhotoLinearParams;
		FilmicTonemappingParams FilmicParams;
		float PhotoLinearScale;
		glm::uvec2 ViewportSize = glm::uvec2(0, 0);
		uint32_t CurrentFrameIndex = 0;
		uint64_t FrameNumber = 0;
		bool bVisualizingCascades = false;
		bool bSoftShadows = true;

		static constexpr size_t BaseVertexBufferSize = 10 * 1024 * 1024; // 10 MB
		static constexpr size_t BaseIndexBufferSize  = 5 * 1024 * 1024; // 5 MB
		static constexpr size_t BaseMaterialBufferSize = 1024 * 1024; // 1 MB

		static constexpr size_t BaseLightsCount = 10;
		static constexpr size_t BasePointLightsBufferSize = BaseLightsCount * sizeof(PointLight);
		static constexpr size_t BaseSpotLightsBufferSize  = BaseLightsCount * sizeof(SpotLight);

		static constexpr uint32_t CSMSizes[EG_CASCADES_COUNT] =
		{
			RendererConfig::DirLightShadowMapSize * 2,
			RendererConfig::DirLightShadowMapSize,
			RendererConfig::DirLightShadowMapSize,
			RendererConfig::DirLightShadowMapSize
		};
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
	static RenderCommandQueue s_ResourceFreeQueue[RendererConfig::FramesInFlight];

	static std::unordered_map<Ref<Shader>, ShaderDependencies> s_ShaderDependencies;

	struct {
		bool operator()(const StaticMeshComponent* a, const StaticMeshComponent* b) const 
		{ return glm::length(s_RendererData->ViewPos - a->GetWorldTransform().Location)
				 < 
				 glm::length(s_RendererData->ViewPos - b->GetWorldTransform().Location); }
	} s_CustomMeshesLess;

	struct {
		bool operator()(const SpriteComponent* a, const SpriteComponent* b) const {
			return glm::length(s_RendererData->ViewPos - a->GetWorldTransform().Location)
				<
				glm::length(s_RendererData->ViewPos - b->GetWorldTransform().Location);
		}
	} s_CustomSpritesLess;

#ifdef EG_GPU_TIMINGS
	struct {
		bool operator()(const GPUTimingData& a, const GPUTimingData& b) const
		{
			return a.Timing > b.Timing;
		}
	} s_CustomGPUTimingsLess;
#endif

	static std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(const glm::mat4& view, const glm::mat4& proj)
	{
		constexpr glm::vec4 frustumCornersNDC[8] =
		{
			{ -1.f, -1.f, +0.f, 1.f },
			{ +1.f, -1.f, +0.f, 1.f },
			{ +1.f, +1.f, +0.f, 1.f },
			{ -1.f, +1.f, +0.f, 1.f },
			{ -1.f, -1.f, +1.f, 1.f },
			{ +1.f, -1.f, +1.f, 1.f },
			{ +1.f, +1.f, +1.f, 1.f },
			{ -1.f, +1.f, +1.f, 1.f },
		};

		const glm::mat4 invViewProj = glm::inverse(proj * view);
		std::array<glm::vec3, 8> frustumCornersWS;
		for (int i = 0; i < 8; ++i)
		{
			const vec4 ws = invViewProj * frustumCornersNDC[i];
			frustumCornersWS[i] = glm::vec3(ws / ws.w);
		}
		return frustumCornersWS;
	}

	glm::vec3 GetFrustumCenter(const std::array<glm::vec3, 8>& frustumCorners)
	{
		glm::vec3 result(0.f);
		for (int i = 0; i < frustumCorners.size(); ++i)
		{
			result += frustumCorners[i];
		}
		result /= float(frustumCorners.size());

		return result;
	}

	static void CreateShadowMapDistribution(uint32_t windowSize, uint32_t filterSize)
	{
		ImageSpecifications specs;
		specs.Size = glm::uvec3((filterSize * filterSize) / 2, windowSize, windowSize);
		specs.Format = ImageFormat::R32G32B32A32_Float;
		specs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
		specs.Layout = ImageReadAccess::PixelShaderRead;
		specs.Type = ImageType::Type3D;
		s_RendererData->ShadowMapDistribution = Image::Create(specs, "Distribution Texture");

		Renderer::Submit([windowSize, filterSize](Ref<CommandBuffer>& cmd) mutable
		{
			const size_t dataSize = windowSize * windowSize * filterSize * filterSize * 2;
			std::vector<float> data(dataSize);

			uint32_t index = 0;
			for (uint32_t y = 0; y < windowSize; ++y)
				for (uint32_t x = 0; x < windowSize; ++x)
					for (int v = int(filterSize) - 1; v >= 0; --v)
						for (uint32_t u = 0; u < filterSize; ++u)
						{
							float x = (float(u) + 0.5f + Random::Float(-0.5f, 0.5f)) / float(filterSize);
							float y = (float(v) + 0.5f + Random::Float(-0.5f, 0.5f)) / float(filterSize);

							EG_ASSERT(index + 1 < data.size());

							constexpr float pi = float(3.14159265358979323846);
							constexpr float _2pi = 2.f * pi;
							const float trigonometryArg = _2pi * x;
							const float sqrtf_y = sqrtf(y);
							data[index] = sqrtf_y * cosf(trigonometryArg);
							data[index + 1] = sqrtf_y * sinf(trigonometryArg);
							index += 2;
						}

			cmd->Write(s_RendererData->ShadowMapDistribution, data.data(), data.size() * sizeof(float), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});
	}

	static Ref<Image> CreateDepthCubeImage(glm::uvec3 size, std::string_view debugName)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
		depthSpecs.bIsCube = true;
		depthSpecs.Size = size;
		return Image::Create(depthSpecs, debugName.data());
	}

	static void SetupPresentPipeline()
	{
		auto& swapchainImages = s_RendererData->Swapchain->GetImages();

		ColorAttachment colorAttachment;
		colorAttachment.Image = swapchainImages[0];
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageLayoutType::Present;
		colorAttachment.bClearEnabled = true;
		colorAttachment.ClearColor = glm::vec4{ 0.f, 0.f, 0.f, 1.f };

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/present.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::None;

		s_RendererData->PresentPipeline = PipelineGraphics::Create(state);

		for (auto& image : swapchainImages)
			s_RendererData->PresentFramebuffers.push_back(Framebuffer::Create({ image }, s_RendererData->ViewportSize, s_RendererData->PresentPipeline->GetRenderPassHandle()));
	}

	static void SetupMeshPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = s_RendererData->GBufferImages.Albedo;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.bClearEnabled = true;

		ColorAttachment geometryNormalAttachment;
		geometryNormalAttachment.Image = s_RendererData->GBufferImages.GeometryNormal;
		geometryNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		geometryNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.bClearEnabled = true;

		ColorAttachment shadingNormalAttachment;
		shadingNormalAttachment.Image = s_RendererData->GBufferImages.ShadingNormal;
		shadingNormalAttachment.InitialLayout = ImageLayoutType::Unknown;
		shadingNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.bClearEnabled = true;

		ColorAttachment materialAttachment;
		materialAttachment.Image = s_RendererData->GBufferImages.MaterialData;
		materialAttachment.InitialLayout = ImageLayoutType::Unknown;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.bClearEnabled = true;

		int objectIDClearColorUint = -1;
		float objectIDClearColor = *(float*)(&objectIDClearColorUint);
		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = s_RendererData->GBufferImages.ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::Unknown;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.bClearEnabled = true;
		objectIDAttachment.ClearColor = glm::vec4{ objectIDClearColor };

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_RendererData->GBufferImages.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.bClearEnabled = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/mesh.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometryNormalAttachment);
		state.ColorAttachments.push_back(shadingNormalAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		s_RendererData->MeshPipeline = PipelineGraphics::Create(state);
	}

	static void SetupPBRPipeline()
	{
		const auto& size = s_RendererData->ViewportSize;

		ImageSpecifications finalColorSpecs;
		finalColorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		finalColorSpecs.Layout = ImageLayoutType::RenderTarget;
		finalColorSpecs.Size = { size.x, size.y, 1 };
		finalColorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		s_RendererData->FinalImage = Image::Create(finalColorSpecs, "Renderer_FinalImage");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R32G32B32A32_Float;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { size.x, size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::Storage;
		s_RendererData->ColorImage = Image::Create(colorSpecs, "Renderer_ColorImage");

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = true;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->ColorImage;

		s_RendererData->PBRFragShader = ShaderLibrary::GetOrLoad("assets/shaders/pbr_shade.frag", ShaderType::Fragment);
		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/pbr_shade.vert", ShaderType::Vertex);
		state.FragmentShader = s_RendererData->PBRFragShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::None;

		s_RendererData->PBRPipeline = PipelineGraphics::Create(state);
	}

	static void SetupBillboardPipeline()
	{
		const auto& size = s_RendererData->ViewportSize;

		ColorAttachment colorAttachment;
		colorAttachment.Image = s_RendererData->ColorImage;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;
		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_RendererData->GBufferImages.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/billboard.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/billboard.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;

		s_RendererData->BillboardData.Pipeline = PipelineGraphics::Create(state);
		s_RendererData->BillboardData.Pipeline->SetBuffer(Buffer::Dummy, 0, 1);
	}

	static void SetupSkyboxPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = false;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->ColorImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_RendererData->GBufferImages.Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.bClearEnabled = false;
		depthAttachment.DepthCompareOp = CompareOperation::LessEqual;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/skybox.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/skybox.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;

		s_RendererData->SkyboxPipeline = PipelineGraphics::Create(state);
	}

	static void SetupIBLPipeline()
	{
		auto vertexShader = ShaderLibrary::GetOrLoad("assets/shaders/ibl.vert", ShaderType::Vertex);

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = true;
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
		colorAttachment.bClearEnabled = true;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->BRDFLUTImage; // just a dummy here

		PipelineGraphicsState brdfLutState;
		brdfLutState.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		brdfLutState.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/brdf_lut.frag", ShaderType::Fragment);
		brdfLutState.ColorAttachments.push_back(colorAttachment);
		brdfLutState.Size = { RendererConfig::BRDFLUTSize, RendererConfig::BRDFLUTSize };

		s_RendererData->BRDFLUTPipeline = PipelineGraphics::Create(brdfLutState);
	}

	static void SetupShadowMapPipeline()
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;

		Ref<Sampler> shadowMapSampler = Sampler::Create(FilterMode::Point, AddressMode::ClampToOpaqueWhite, CompareOperation::Never, 0.f, 0.f, 1.f);
		for (uint32_t i = 0; i < s_RendererData->DirectionalLightShadowMaps.size(); ++i)
		{
			depthSpecs.Size = glm::uvec3(RendererData::CSMSizes[i], RendererData::CSMSizes[i], 1);
			s_RendererData->DirectionalLightShadowMaps[i] = Image::Create(depthSpecs, std::string("CSMShadowMap") + std::to_string(i));
			s_RendererData->DirectionalLightShadowMapSamplers[i] = shadowMapSampler;
		}

		// Transition in case we don't run render pass
		// Otherwise we get VK validation errors
		Renderer::Submit([](Ref<CommandBuffer>& cmd)
		{
			for (auto& image : s_RendererData->DirectionalLightShadowMaps)
				cmd->TransitionLayout(image, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		depthAttachment.Image = s_RendererData->DirectionalLightShadowMaps[0];
		depthAttachment.bWriteDepth = true;
		depthAttachment.bClearEnabled = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		s_RendererData->ShadowMapPipeline = PipelineGraphics::Create(state);

		const void* renderPassHandle = s_RendererData->ShadowMapPipeline->GetRenderPassHandle();
		for (uint32_t i = 0; i < s_RendererData->ShadowMapFramebuffers.size(); ++i)
			s_RendererData->ShadowMapFramebuffers[i] = Framebuffer::Create({ s_RendererData->DirectionalLightShadowMaps[i] }, glm::uvec2(RendererData::CSMSizes[i]), renderPassHandle);


		// For point lights
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageLayoutType::Unknown;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = s_RendererData->DummyCubeDepthImage;
			depthAttachment.bClearEnabled = true;

			ShaderDefines defines;
			defines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			s_RendererData->PointLightSMPipeline = PipelineGraphics::Create(state);
			std::fill(s_RendererData->PointLightShadowMapSamplers.begin(), s_RendererData->PointLightShadowMapSamplers.end(), shadowMapSampler);
		}
	}

	static void SetupPostProcessingPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_RendererData->FinalImage;
		colorAttachment.bClearEnabled = true;
		colorAttachment.ClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/present.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/postprocessing.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);

		s_RendererData->PostProcessingPipeline = PipelineGraphics::Create(state);
	}

	static void UpdateIndexBuffer(Ref<CommandBuffer>& cmd, Ref<Buffer>& indexBuffer)
	{
		const size_t ibSize = indexBuffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size(); i += 6)
		{
			indices[i + 0] = offset + 2;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 0;

			indices[i + 3] = offset + 0;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 2;

			offset += 4;
		}

		cmd->Write(indexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
	}

	// Tries to add texture to the system. Returns its index in vector<Ref<Image>> Images
	size_t Renderer::AddTexture(const Ref<Texture>& texture)
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
			s_RendererData->TextureMapChangedFrame = s_RendererData->FrameNumber;
			return index;
		}

		return it->second;
	}

	void Renderer::Init()
	{
		s_RendererData = new RendererData();
		s_RendererData->Swapchain = Application::Get().GetWindow().GetSwapchain();
		s_RendererData->ViewportSize = s_RendererData->Swapchain->GetSize();

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
		Texture2D::MeshIconTexture = Texture2D::Create("assets/textures/Editor/meshicon.png", {}, false);
		Texture2D::TextureIconTexture = Texture2D::Create("assets/textures/Editor/textureicon.png", {}, false);
		Texture2D::SceneIconTexture = Texture2D::Create("assets/textures/Editor/sceneicon.png", {}, false);
		Texture2D::SoundIconTexture = Texture2D::Create("assets/textures/Editor/soundicon.png", {}, false);
		Texture2D::FolderIconTexture = Texture2D::Create("assets/textures/Editor/foldericon.png", {}, false);
		Texture2D::UnknownIconTexture = Texture2D::Create("assets/textures/Editor/unknownicon.png", {}, false);
		Texture2D::PlayButtonTexture = Texture2D::Create("assets/textures/Editor/playbutton.png", {}, false);
		Texture2D::StopButtonTexture = Texture2D::Create("assets/textures/Editor/stopbutton.png", {}, false);
		Texture2D::PointLightIcon = Texture2D::Create("assets/textures/Editor/pointlight.png", {}, false);
		Texture2D::DirectionalLightIcon = Texture2D::Create("assets/textures/Editor/directionallight.png", {}, false);
		Texture2D::SpotLightIcon = Texture2D::Create("assets/textures/Editor/spotlight.png", {}, false);

		BufferSpecifications dummyBufferSpecs;
		dummyBufferSpecs.Size = 4;
		dummyBufferSpecs.Usage = BufferUsage::StorageBuffer;
		Buffer::Dummy = Buffer::Create(dummyBufferSpecs, "DummyBuffer");

		Sampler::PointSampler = Sampler::Create(FilterMode::Point, AddressMode::Wrap, CompareOperation::Never, 0.f, 0.f, 1.f);
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

		s_RendererData->DummyCubeDepthImage = CreateDepthCubeImage(glm::uvec3{ 1, 1, 1 }, "DummyCubeDepthImage");
		s_RendererData->PointLightSMFramebuffers.reserve(256);
		std::fill(s_RendererData->PointLightShadowMaps.begin(), s_RendererData->PointLightShadowMaps.end(), s_RendererData->DummyCubeDepthImage);

		// Init renderer pipelines
		s_RendererData->GBufferImages.Init({ s_RendererData->ViewportSize, 1 });
		SetupPresentPipeline();
		SetupMeshPipeline();
		SetupPBRPipeline();
		SetupBillboardPipeline();
		SetupSkyboxPipeline();
		SetupIBLPipeline();
		SetupBRDFLUTPipeline();
		SetupShadowMapPipeline();
		SetupPostProcessingPipeline();

		// Init renderer settings
		SetPhotoLinearTonemappingParams(s_RendererData->PhotoLinearParams);
		SetSoftShadowsEnabled(s_RendererData->bSoftShadows);

		s_RendererData->DummyIBL = TextureCube::Create(Texture2D::BlackTexture, 1);

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = BillboardData::BaseBillboardVertexBufferSize;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = BillboardData::BaseBillboardIndexBufferSize;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;
		s_RendererData->BillboardData.VertexBuffer = Buffer::Create(vertexSpecs, "BillboardVertexBuffer_2D");
		s_RendererData->BillboardData.IndexBuffer = Buffer::Create(indexSpecs, "BillboardIndexBuffer_2D");
		s_RendererData->BillboardData.Vertices.reserve(BillboardData::DefaultBillboardVerticesCount);

		Renderer::Submit([](Ref<CommandBuffer>& cmd)
		{
			UpdateIndexBuffer(cmd, s_RendererData->BillboardData.IndexBuffer);
			cmd->TransitionLayout(s_RendererData->DummyCubeDepthImage, ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
		});

		CreateBuffers();

		//Renderer3D Init
		s_RendererData->Sprites.reserve(1'000);
		s_RendererData->Lines.reserve(100);
		s_RendererData->ShaderMaterials.reserve(100);
		s_RendererData->Miscellaneous.reserve(25);

		// Init textures & Fill with dummy textures
		s_RendererData->Images.resize(EG_MAX_TEXTURES);
		s_RendererData->Samplers.resize(EG_MAX_TEXTURES);
		std::fill(s_RendererData->Images.begin(), s_RendererData->Images.end(), Texture2D::DummyTexture->GetImage());
		std::fill(s_RendererData->Samplers.begin(), s_RendererData->Samplers.end(), Texture2D::DummyTexture->GetSampler());

		//Renderer2D Init
		Renderer2D::Init(s_RendererData->GBufferImages);

		// Render BRDF LUT
		{
			Renderer::Submit([](Ref<CommandBuffer>& cmd)
			{
				struct PushData
				{
					uint32_t FlipX = 0;
					uint32_t FlipY = 1;
				} pushData;
				Ref<Framebuffer> framebuffer = Framebuffer::Create({ s_RendererData->BRDFLUTImage }, s_RendererData->BRDFLUTImage->GetSize(), s_RendererData->BRDFLUTPipeline->GetRenderPassHandle());
				cmd->BeginGraphics(s_RendererData->BRDFLUTPipeline, framebuffer);
				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->Draw(6, 0);
				cmd->EndGraphics();
			});
		}

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		cmd->End();
		Renderer::SubmitCommandBuffer(cmd, true);
	}

	void Renderer::CreateBuffers()
	{
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

		BufferSpecifications cameraViewDataBufferSpecs;
		cameraViewDataBufferSpecs.Size = sizeof(glm::mat4);
		cameraViewDataBufferSpecs.Usage = BufferUsage::UniformBuffer | BufferUsage::TransferDst;

		BufferSpecifications pointLightsBufferSpecs;
		pointLightsBufferSpecs.Size = s_RendererData->BasePointLightsBufferSize;
		pointLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications spotLightsBufferSpecs;
		spotLightsBufferSpecs.Size = s_RendererData->BaseSpotLightsBufferSize;
		spotLightsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications directionalLightBufferSpecs;
		directionalLightBufferSpecs.Size = sizeof(DirectionalLight);
		directionalLightBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		BufferSpecifications pointLightsVPBufferSpecs;
		pointLightsVPBufferSpecs.Size = sizeof(glm::mat4) * 6;
		pointLightsVPBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;

		s_RendererData->VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer");
		s_RendererData->IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer");
		s_RendererData->MaterialBuffer = Buffer::Create(materialsBufferSpecs, "MaterialsBuffer");
		s_RendererData->AdditionalMeshDataBuffer = Buffer::Create(additionalMeshDataBufferSpecs, "AdditionalMeshData");
		s_RendererData->CameraViewDataBuffer = Buffer::Create(cameraViewDataBufferSpecs, "CameraViewData");
		s_RendererData->PointLightsVPsBuffer = Buffer::Create(pointLightsVPBufferSpecs, "PointLightsVPs");

		s_RendererData->PointLightsBuffer = Buffer::Create(pointLightsBufferSpecs, "PointLightsBuffer");
		s_RendererData->SpotLightsBuffer = Buffer::Create(spotLightsBufferSpecs, "SpotLightsBuffer");
		s_RendererData->DirectionalLightBuffer = Buffer::Create(directionalLightBufferSpecs, "DirectionalLightBuffer");
	}
	
	bool Renderer::UsedTextureChanged()
	{
		return s_RendererData->bTextureMapChanged;
	}

	uint64_t Renderer::UsedTextureChangedFrame()
	{
		return s_RendererData->TextureMapChangedFrame;
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

	const std::vector<Ref<Framebuffer>>& Renderer::GetCSMFramebuffers()
	{
		return s_RendererData->ShadowMapFramebuffers;
	}

	const DirectionalLight& Renderer::GetDirectionalLight(bool* hasDirLight)
	{
		*hasDirLight = s_RendererData->HasDirectionalLight;
		return s_RendererData->DirectionalLight;
	}

	const std::vector<PointLight>& Renderer::GetPointLights()
	{
		return s_RendererData->PointLights;
	}

	Ref<Buffer>& Renderer::GetPointLightsVPBuffer()
	{
		return s_RendererData->PointLightsVPsBuffer;
	}

	const std::vector<Ref<Image>>& Renderer::GetPointLightsShadowMaps()
	{
		return s_RendererData->PointLightShadowMaps;
	}

	Ref<Image>& Renderer::GetDummyDepthCubeImage()
	{
		return s_RendererData->DummyCubeDepthImage;
	}

	void Renderer::Shutdown()
	{
#ifdef EG_GPU_TIMINGS
		s_RendererData->RHIGPUTimings.clear();
#endif
		s_RendererData->GPUTimings.clear();
		s_ShaderDependencies.clear();

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
		Texture2D::PointLightIcon.reset();
		Texture2D::DirectionalLightIcon.reset();
		Texture2D::SpotLightIcon.reset();
		Buffer::Dummy.reset();

		Sampler::PointSampler.reset();
		Sampler::TrilinearSampler.reset();

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

		for (uint32_t i = 0; i < RendererConfig::FramesInFlight; ++i)
			Renderer::GetResourceReleaseQueue(i).Execute();
	}

	void Renderer::BeginFrame()
	{
		s_RendererData->GPUTimings.clear();
#ifdef EG_GPU_TIMINGS
		for (auto it = s_RendererData->RHIGPUTimings.begin(); it != s_RendererData->RHIGPUTimings.end();)
		{
			// If it wasn't used in prev frame, erase it
			if (it->second->bIsUsed == false)
				it = s_RendererData->RHIGPUTimings.erase(it);
			else
			{
				it->second->QueryTiming(s_RendererData->CurrentFrameIndex);
				s_RendererData->GPUTimings.push_back({ it->first, it->second->GetTiming() });
				it->second->bIsUsed = false; // Set to false for the current frame
				++it;
			}
		}
		std::sort(s_RendererData->GPUTimings.begin(), s_RendererData->GPUTimings.end(), s_CustomGPUTimingsLess);
#endif
		EG_CPU_TIMING_SCOPED("Renderer::BeginFrame");
		StagingManager::NextFrame();
		s_RendererData->ImGuiLayer = &Application::Get().GetImGuiLayer();
	}

	void Renderer::EndFrame()
	{
		EG_CPU_TIMING_SCOPED("Renderer::EndFrame");

		auto& fence = s_RendererData->Fences[s_RendererData->CurrentFrameIndex];
		auto& semaphore = s_RendererData->Semaphores[s_RendererData->CurrentFrameIndex];
		fence->Wait();

		uint32_t imageIndex = 0;
		auto imageAcquireSemaphore = s_RendererData->Swapchain->AcquireImage(&imageIndex);

		Renderer::Submit([imageIndex, data = s_RendererData](Ref<CommandBuffer>& cmd)
		{
			struct PushData
			{
				uint32_t FlipX = 0;
				uint32_t FlipY = 0;
			} pushData;

			// Drawing UI if build with editor. Otherwise just copy final render to present
#ifdef EG_WITH_EDITOR
			EG_GPU_TIMING_SCOPED(cmd, "Present+ImGui");

			cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[imageIndex]);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			(*data->ImGuiLayer)->End(cmd);
			cmd->EndGraphics();
			(*data->ImGuiLayer)->UpdatePlatform();
#else
			EG_GPU_TIMING_SCOPED(cmd, "Present");

			data->PresentPipeline->SetImageSampler(data->ColorImage, Sampler::PointSampler, 0, 0);
			cmd->BeginGraphics(data->PresentPipeline, data->PresentFramebuffers[imageIndex]);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
#endif
		});

		auto& cmd = GetCurrentFrameCommandBuffer();
		cmd->Begin();
		s_CommandQueue.Execute();
		s_ResourceFreeQueue[s_RendererData->CurrentFrameIndex].Execute();
		cmd->End();
		fence->Reset();
		s_RendererData->GraphicsCommandManager->Submit(cmd.get(), 1, fence, imageAcquireSemaphore.get(), 1, semaphore.get(), 1);
		s_RendererData->Swapchain->Present(semaphore);

		s_RendererData->CurrentFrameIndex = (s_RendererData->CurrentFrameIndex + 1) % RendererConfig::FramesInFlight;
		s_RendererData->FrameNumber++;
		s_RendererData->bTextureMapChanged = false;

		s_RendererData->BillboardData.Vertices.clear();

		// Reset stats of the next frame
		Renderer::ResetStats();
		Renderer2D::ResetStats();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return s_CommandQueue;
	}

	void Renderer::BeginScene(const CameraComponent& cameraComponent, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		s_RendererData->Camera = &cameraComponent.Camera;
		const glm::mat4& cameraView = cameraComponent.GetViewMatrix();
		const glm::mat4& cameraProjection = cameraComponent.Camera.GetProjection();

		s_RendererData->CurrentFrameView = cameraView;
		s_RendererData->CurrentFrameProj = cameraProjection;
		s_RendererData->CurrentFrameViewProj = cameraProjection * cameraView;
		s_RendererData->CurrentFrameInvViewProj = glm::inverse(s_RendererData->CurrentFrameViewProj);
		s_RendererData->ViewPos = cameraComponent.GetWorldTransform().Location;
		UpdateLightsBuffers(pointLights, directionalLight, spotLights);
	}

	void Renderer::BeginScene(const EditorCamera& editorCamera, const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLight, const std::vector<SpotLightComponent*>& spotLights)
	{
		s_RendererData->Camera = &editorCamera;
		const glm::mat4& cameraView = editorCamera.GetViewMatrix();
		const glm::mat4& cameraProjection = editorCamera.GetProjection();

		s_RendererData->CurrentFrameView = cameraView;
		s_RendererData->CurrentFrameProj = cameraProjection;
		s_RendererData->CurrentFrameViewProj = cameraProjection * cameraView;
		s_RendererData->CurrentFrameInvViewProj = glm::inverse(s_RendererData->CurrentFrameViewProj);
		s_RendererData->ViewPos = editorCamera.GetLocation();
		UpdateLightsBuffers(pointLights, directionalLight, spotLights);
	}

	void Renderer::UpdateLightsBuffers(const std::vector<PointLightComponent*>& pointLights, const DirectionalLightComponent* directionalLightComponent, const std::vector<SpotLightComponent*>& spotLights)
	{
		EG_CPU_TIMING_SCOPED("Renderer::UpdateLightsBuffers");

		s_RendererData->PointLights.clear();
		s_RendererData->SpotLights.clear();

		constexpr glm::vec3 directions[6] = { glm::vec3(1.0, 0.0, 0.0), glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
						                      glm::vec3(0.0,-1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),  glm::vec3(0.0, 0.0,-1.0) };

		constexpr glm::vec3 upVectors[6]  = { glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),
											  glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0) };

		static const glm::mat4 pointLightPerspectiveProjection = glm::perspective(glm::radians(90.f), 1.f, EG_POINT_LIGHT_NEAR, EG_POINT_LIGHT_FAR);

		for (auto& pointLight : pointLights)
		{
			PointLight light;
			light.Position   = pointLight->GetWorldTransform().Location;
			light.LightColor = pointLight->LightColor;
			light.Intensity  = glm::max(pointLight->Intensity, 0.0f);

			for (int i = 0; i < 6; ++i)
				light.ViewProj[i] = pointLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + directions[i], upVectors[i]);

			s_RendererData->PointLights.push_back(light);
		}

		for (auto& spotLight : spotLights)
		{
			SpotLight light;
			light.Position = spotLight->GetWorldTransform().Location;
			light.Direction = spotLight->GetForwardVector();

			light.LightColor = spotLight->LightColor;
			light.InnerCutOffAngle = spotLight->InnerCutOffAngle;
			light.OuterCutOffAngle = spotLight->OuterCutOffAngle;
			light.Intensity = glm::max(spotLight->Intensity, 0.f);

			glm::mat4 spotLightPerspectiveProjection = glm::perspective(glm::radians(glm::min(light.OuterCutOffAngle * 2.f, 179.f)), 1.f, 0.01f, 10000.f);
			spotLightPerspectiveProjection[1][1] *= -1.f;
			light.ViewProj = spotLightPerspectiveProjection * glm::lookAt(light.Position, light.Position + light.Direction, glm::vec3{ 0.f, 1.f, 0.f });

			s_RendererData->SpotLights.push_back(light);
		}

		s_RendererData->HasDirectionalLight = directionalLightComponent != nullptr;
		if (s_RendererData->HasDirectionalLight)
		{
			auto& directionalLight = s_RendererData->DirectionalLight;
			directionalLight.Direction = directionalLightComponent->GetForwardVector();
			directionalLight.Ambient = directionalLightComponent->Ambient;
			directionalLight.LightColor = directionalLightComponent->LightColor;
			for (uint32_t i = 0; i < EG_CASCADES_COUNT; ++i)
				directionalLight.CascadePlaneDistances[i] = s_RendererData->Camera->GetCascadeFarPlane(i);

			// https://alextardif.com/shadowmapping.html is used as a ref for CSM
			// Creating base lookAt using light dir
			constexpr glm::vec3 upDir = glm::vec3(0.f, 1.f, 0.f);
			const glm::vec3 baseLookAt = -directionalLight.Direction;
			const glm::mat4 defaultLookAt = glm::lookAt(glm::vec3(0), baseLookAt, upDir);

			for (uint32_t index = 0; index < EG_CASCADES_COUNT; ++index)
			{
				const glm::mat4& cascadeProj = s_RendererData->Camera->GetCascadeProjection(index);
				const std::array frustumCorners = GetFrustumCornersWorldSpace(s_RendererData->CurrentFrameView, cascadeProj);
				glm::vec3 frustumCenter = GetFrustumCenter(frustumCorners);
				// Take the farthest corners, subtract, get length
				const float diameter = glm::length(frustumCorners[0] - frustumCorners[6]);
				const float radius = diameter * 0.5f;
				const float texelsPerUnit = float(s_RendererData->CSMSizes[index]) / diameter;

				const glm::mat4 scaling = glm::scale(glm::mat4(1.f), glm::vec3(texelsPerUnit));
				
				glm::mat4 lookAt = defaultLookAt * scaling;
				const glm::mat4 invLookAt = glm::inverse(lookAt);

				// Move our frustum center in texel-sized increments, then get it back into its original space
				frustumCenter = lookAt * glm::vec4(frustumCenter, 1.f);
				frustumCenter.x = glm::floor(frustumCenter.x);
				frustumCenter.y = glm::floor(frustumCenter.y);
				frustumCenter = invLookAt * glm::vec4(frustumCenter, 1.f);

				// Creating our new eye by moving towards the opposite direction of the light by the diameter
				const glm::vec3 frustumCenter3 = glm::vec3(frustumCenter);
				glm::vec3 eye = frustumCenter3 - (directionalLight.Direction * diameter);

				// Final light view matrix
				const glm::mat4 lightView = glm::lookAt(eye, frustumCenter3, upDir);

				// Final light proj matrix that keeps a consistent size. Multiplying by 6 is not perfect.
				// Near and far should be calculating using scene bounds, but for now it'll be like that.
				const glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, -radius * 6.f, radius * 6.f);

				directionalLight.ViewProj[index] = lightProj * lightView;
			}
		}

		Renderer::Submit([](Ref<CommandBuffer>& cmd)
		{
			EG_CPU_TIMING_SCOPED("Renderer::Submit. Upload LightBuffers");
			EG_GPU_TIMING_SCOPED(cmd, "Upload LightBuffers");

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
			cmd->Write(s_RendererData->DirectionalLightBuffer, &s_RendererData->DirectionalLight, sizeof(DirectionalLight), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->PointLightsBuffer, EG_SCENE_SET, EG_BINDING_POINT_LIGHTS);
			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->SpotLightsBuffer, EG_SCENE_SET, EG_BINDING_SPOT_LIGHTS);
			s_RendererData->PBRPipeline->SetBuffer(s_RendererData->DirectionalLightBuffer, EG_SCENE_SET, EG_BINDING_DIRECTIONAL_LIGHT);
		});
	}

	void Renderer::UpdateBuffers(Ref<CommandBuffer>& cmd, const std::vector<const StaticMeshComponent*>& meshes)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Upload Vertex & Index buffers");
		EG_CPU_TIMING_SCOPED("Renderer. Upload Vertex & Index buffers");

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = 0;
		size_t currentIndexSize = 0;
		for (auto& mesh : meshes)
		{
			currentVertexSize += mesh->StaticMesh->GetVerticesCount() * sizeof(Vertex);
			currentIndexSize  += mesh->StaticMesh->GetIndecesCount() * sizeof(Index);
		}

		if (currentVertexSize > s_RendererData->VertexBuffer->GetSize())
		{
			currentVertexSize = (currentVertexSize * 3) / 2;
			s_RendererData->VertexBuffer->Resize(currentVertexSize);
		}
		if (currentIndexSize > s_RendererData->IndexBuffer->GetSize())
		{
			currentIndexSize = (currentIndexSize * 3) / 2;
			s_RendererData->IndexBuffer->Resize(currentIndexSize);
		}

		static std::vector<Vertex> vertices;
		static std::vector<Index> indices;
		vertices.clear(); vertices.reserve(currentVertexSize);
		indices.clear(); indices.reserve(currentIndexSize);

		for (auto& mesh : meshes)
		{
			const auto& meshVertices = mesh->StaticMesh->GetVertices();
			const auto& meshIndices = mesh->StaticMesh->GetIndeces();
			vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
			indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());
		}

		auto& vb = s_RendererData->VertexBuffer;
		auto& ib = s_RendererData->IndexBuffer;

		cmd->Write(vb, vertices.data(), vertices.size() * sizeof(Vertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->Write(ib, indices.data(), indices.size() * sizeof(Index), 0, BufferLayoutType::Unknown, BufferReadAccess::Index);

		auto& additionalBuffer = s_RendererData->AdditionalMeshDataBuffer;
		auto& cameraViewBuffer = s_RendererData->CameraViewDataBuffer;

		g_AdditionalMeshData.ViewProjection = s_RendererData->CurrentFrameViewProj;
		cmd->Write(additionalBuffer, &g_AdditionalMeshData, sizeof(AdditionalMeshData), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
		s_RendererData->MeshPipeline->SetBuffer(additionalBuffer, EG_PERSISTENT_SET, EG_BINDING_MAX);

		cmd->Write(cameraViewBuffer, &s_RendererData->CurrentFrameView[0][0], sizeof(glm::mat4), 0, BufferLayoutType::Unknown, BufferReadAccess::Uniform);
		s_RendererData->PBRPipeline->SetBuffer(cameraViewBuffer, EG_SCENE_SET, EG_BINDING_CAMERA_VIEW);
	}

	void Renderer::EndScene()
	{
		EG_CPU_TIMING_SCOPED("Renderer::EndScene");

		std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

		//Sorting from front to back
		std::sort(std::begin(s_RendererData->Sprites), std::end(s_RendererData->Sprites), s_CustomSpritesLess);
		std::sort(std::begin(s_RendererData->Meshes), std::end(s_RendererData->Meshes), s_CustomMeshesLess);

		UpdateMaterials();
		RenderMeshes();
		RenderSprites();
		PBRPass();
		RenderBillboards();
		SkyboxPass();
		PostprocessingPass();

		std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
		s_RendererData->Stats[s_RendererData->CurrentFrameIndex].RenderingTook = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.f;

		s_RendererData->Meshes.clear();
		s_RendererData->Sprites.clear();
		s_RendererData->Lines.clear();
		s_RendererData->Miscellaneous.clear();
	}

	void Renderer::ShadowPass(Ref<CommandBuffer>& cmd, const std::vector<const StaticMeshComponent*>& meshes)
	{
		if (meshes.empty())
			return;

		const auto& dirLight = s_RendererData->DirectionalLight;
		struct VertexPushData
		{
			glm::mat4 Model;
			glm::mat4 ViewProj;
		} pushData;

		// For directional light
		if (s_RendererData->HasDirectionalLight)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Meshes: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Renderer, meshes. CSM Shadow pass");

			for (uint32_t i = 0; i < s_RendererData->ShadowMapFramebuffers.size(); ++i)
			{
				uint32_t firstIndex = 0;
				uint32_t vertexOffset = 0;
				pushData.ViewProj = dirLight.ViewProj[i];

				cmd->BeginGraphics(s_RendererData->ShadowMapPipeline, s_RendererData->ShadowMapFramebuffers[i]);
				for (auto& mesh : meshes)
				{
					const auto& vertices = mesh->StaticMesh->GetVertices();
					const auto& indices = mesh->StaticMesh->GetIndeces();
					size_t vertexSize = vertices.size() * sizeof(Vertex);
					size_t indexSize = indices.size() * sizeof(Index);

					// TODO: optimize
					pushData.Model = Math::ToTransformMatrix(mesh->GetWorldTransform());

					cmd->SetGraphicsRootConstants(&pushData, nullptr);
					cmd->DrawIndexed(s_RendererData->VertexBuffer, s_RendererData->IndexBuffer, (uint32_t)indices.size(), firstIndex, vertexOffset);
					firstIndex += (uint32_t)indices.size();
					vertexOffset += (uint32_t)vertices.size();
				}
				cmd->EndGraphics();
			}
		}

		// For point lights
		if (s_RendererData->PointLights.size())
		{
			auto& vpsBuffer = s_RendererData->PointLightsVPsBuffer;
			auto& pipeline = s_RendererData->PointLightSMPipeline;
			auto& framebuffers = s_RendererData->PointLightSMFramebuffers;
			pipeline->SetBuffer(vpsBuffer, 0, 0);
			{
				EG_GPU_TIMING_SCOPED(cmd, "Meshes: Point Lights Shadow pass");
				EG_CPU_TIMING_SCOPED("Renderer, meshes. Point Lights Shadow pass");

				uint32_t i = 0;
				for (auto& pointLight : s_RendererData->PointLights)
				{
					if (i >= framebuffers.size())
					{
						// Create SM & framebuffer
						s_RendererData->PointLightShadowMaps[i] = CreateDepthCubeImage(RendererConfig::PointLightSMSize, "PointLight_SM_" + std::to_string(i));
						framebuffers.push_back(Framebuffer::Create({ s_RendererData->PointLightShadowMaps[i] }, glm::uvec2(RendererConfig::PointLightSMSize), pipeline->GetRenderPassHandle()));
					}

					cmd->StorageBufferBarrier(vpsBuffer);
					cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
					cmd->StorageBufferBarrier(vpsBuffer);

					uint32_t firstIndex = 0;
					uint32_t vertexOffset = 0;

					cmd->BeginGraphics(pipeline, framebuffers[i]);
					for (auto& mesh : meshes)
					{
						const auto& vertices = mesh->StaticMesh->GetVertices();
						const auto& indices = mesh->StaticMesh->GetIndeces();
						size_t vertexSize = vertices.size() * sizeof(Vertex);
						size_t indexSize = indices.size() * sizeof(Index);

						// TODO: optimize
						pushData.Model = Math::ToTransformMatrix(mesh->GetWorldTransform());

						cmd->SetGraphicsRootConstants(&pushData, nullptr);
						cmd->DrawIndexed(s_RendererData->VertexBuffer, s_RendererData->IndexBuffer, (uint32_t)indices.size(), firstIndex, vertexOffset);
						firstIndex += (uint32_t)indices.size();
						vertexOffset += (uint32_t)vertices.size();
					}
					cmd->EndGraphics();
					++i;
				}
			}
		}
	}

	void Renderer::PBRPass()
	{
		Renderer::Submit([data = s_RendererData, iblTexture = s_RendererData->IBLTexture](Ref<CommandBuffer>& cmd)
		{
			EG_GPU_TIMING_SCOPED(cmd, "PBR Pass");
			EG_CPU_TIMING_SCOPED("Renderer. PBR Pass");

			struct PushData
			{
				glm::mat4 ViewProjInv;
				glm::vec3 CameraPos;
				uint32_t PointLightsCount;
				uint32_t SpotLightsCount;
				uint32_t HasDirectionalLight;
				float Gamma;
				float MaxReflectionLOD;
				uint32_t bHasIrradiance;
			} pushData;
			static_assert(sizeof(PushData) <= 128);

			const bool bHasIrradiance = iblTexture.operator bool();
			auto& ibl = bHasIrradiance ? iblTexture : data->DummyIBL;

			pushData.ViewProjInv = data->CurrentFrameInvViewProj;
			pushData.CameraPos = data->ViewPos;
			pushData.PointLightsCount = (uint32_t)data->PointLights.size();
			pushData.SpotLightsCount = (uint32_t)data->SpotLights.size();
			pushData.HasDirectionalLight = s_RendererData->HasDirectionalLight ? 1 : 0;
			pushData.Gamma = data->Gamma;
			pushData.MaxReflectionLOD = float(ibl->GetPrefilterImage()->GetMipsCount() - 1);
			pushData.bHasIrradiance = bHasIrradiance;

			const Ref<Image>& smDistribution = s_RendererData->bSoftShadows ? data->ShadowMapDistribution : Texture2D::DummyTexture->GetImage();
			data->PBRPipeline->SetImageSampler(data->GBufferImages.Albedo,            Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_ALBEDO_TEXTURE);
			data->PBRPipeline->SetImageSampler(data->GBufferImages.GeometryNormal,    Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_GEOMETRY_NORMAL_TEXTURE);
			data->PBRPipeline->SetImageSampler(data->GBufferImages.ShadingNormal,     Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SHADING_NORMAL_TEXTURE);
			data->PBRPipeline->SetImageSampler(data->GBufferImages.Depth,             Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_DEPTH_TEXTURE);
			data->PBRPipeline->SetImageSampler(data->GBufferImages.MaterialData,      Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_MATERIAL_DATA_TEXTURE);
			data->PBRPipeline->SetImageSampler(ibl->GetIrradianceImage(),             Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_IRRADIANCE_MAP);
			data->PBRPipeline->SetImageSampler(ibl->GetPrefilterImage(),              ibl->GetPrefilterImageSampler(), EG_SCENE_SET, EG_BINDING_PREFILTER_MAP);
			data->PBRPipeline->SetImageSampler(data->BRDFLUTImage,                    Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_BRDF_LUT);
			data->PBRPipeline->SetImageSampler(smDistribution,                        Sampler::PointSampler, EG_SCENE_SET, EG_BINDING_SM_DISTRIBUTION);
			data->PBRPipeline->SetImageSamplerArray(data->PointLightShadowMaps,       data->PointLightShadowMapSamplers, EG_SCENE_SET, EG_BINDING_SM_POINT_LIGHT);
			data->PBRPipeline->SetImageSamplerArray(data->DirectionalLightShadowMaps, data->DirectionalLightShadowMapSamplers, EG_SCENE_SET, EG_BINDING_CSM_SHADOW_MAPS);

			cmd->TransitionLayout(data->GBufferImages.Depth, data->GBufferImages.Depth->GetLayout(), ImageReadAccess::PixelShaderRead);
			cmd->BeginGraphics(data->PBRPipeline);
			cmd->SetGraphicsRootConstants(nullptr, &pushData);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
			cmd->TransitionLayout(data->GBufferImages.Depth, data->GBufferImages.Depth->GetLayout(), ImageLayoutType::DepthStencilWrite);
		});
	}

	void Renderer::SkyboxPass()
	{
		if (!s_RendererData->IBLTexture)
			return;

		Renderer::Submit([data = s_RendererData, ibl = std::move(s_RendererData->IBLTexture)](Ref<CommandBuffer>& cmd)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Skybox Pass");
			EG_CPU_TIMING_SCOPED("Renderer. Skybox Pass");


		    glm::mat4 ViewProj = data->CurrentFrameProj * glm::mat4(glm::mat3(data->CurrentFrameView));

			data->SkyboxPipeline->SetImageSampler(ibl->GetImage(), Sampler::PointSampler, 0, 0);
			cmd->BeginGraphics(data->SkyboxPipeline);
			cmd->SetGraphicsRootConstants(&ViewProj[0][0], nullptr);
			cmd->Draw(36, 0);
			cmd->EndGraphics();
		});
	}

	void Renderer::PostprocessingPass()
	{
		Renderer::Submit([size = s_RendererData->ViewportSize](Ref<CommandBuffer>& cmd)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Postprocessing pass");
			EG_CPU_TIMING_SCOPED("Renderer. Postprocessing Pass");


			struct VertexPushData
			{
				uint32_t FlipX = 0;
				uint32_t FlipY = 0;
			} vertexPushData;

			struct PushData
			{
			    uint32_t Width, Height;
				float InvGamma;
				float Exposure;
				float PhotolinearScale;
				float WhitePoint;
				uint32_t TonemappingMethod;
		    } pushData;
			static_assert(sizeof(VertexPushData) + sizeof(PushData) <= 128);

		    pushData.Width = size.x;
			pushData.Height = size.y;
			pushData.InvGamma = 1.f / s_RendererData->Gamma;
			pushData.Exposure = s_RendererData->Exposure;
			pushData.PhotolinearScale = s_RendererData->PhotoLinearScale;
			pushData.WhitePoint = s_RendererData->FilmicParams.WhitePoint;
			pushData.TonemappingMethod = (uint32_t)s_RendererData->TonemappingMethod;

			s_RendererData->PostProcessingPipeline->SetImageSampler(s_RendererData->ColorImage, Sampler::PointSampler, 0, 0);

			cmd->BeginGraphics(s_RendererData->PostProcessingPipeline);
			cmd->SetGraphicsRootConstants(&vertexPushData, &pushData);
			cmd->Draw(6, 0);
			cmd->EndGraphics();
		});
	}

	void Renderer::UpdateMaterials()
	{
		EG_CPU_TIMING_SCOPED("Renderer. Update Materials");

		s_RendererData->ShaderMaterials.clear();
		for (auto& mesh : s_RendererData->Meshes)
		{
			uint32_t albedoTextureIndex = (uint32_t)AddTexture(mesh->Material->GetAlbedoTexture());
			uint32_t metallnessTextureIndex = (uint32_t)AddTexture(mesh->Material->GetMetallnessTexture());
			uint32_t normalTextureIndex = (uint32_t)AddTexture(mesh->Material->GetNormalTexture());
			uint32_t roughnessTextureIndex = (uint32_t)AddTexture(mesh->Material->GetRoughnessTexture());
			uint32_t aoTextureIndex = (uint32_t)AddTexture(mesh->Material->GetAOTexture());

			CPUMaterial material;
			material.PackedTextureIndices = material.PackedTextureIndices2 = 0;

			material.PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
			material.PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
			material.PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

			material.PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
			material.PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

			s_RendererData->ShaderMaterials.push_back(material);
		}
		for (auto& sprite : s_RendererData->Sprites)
		{
			AddTexture(sprite->Material->GetAlbedoTexture());
			AddTexture(sprite->Material->GetMetallnessTexture());
			AddTexture(sprite->Material->GetNormalTexture());
			AddTexture(sprite->Material->GetRoughnessTexture());
			AddTexture(sprite->Material->GetAOTexture());
		}
		if (s_RendererData->bTextureMapChanged)
		{
			s_RendererData->MeshPipeline->SetImageSamplerArray(s_RendererData->Images, s_RendererData->Samplers, EG_PERSISTENT_SET, EG_BINDING_TEXTURES);
		}
	}

	void Renderer::RenderMeshes()
	{
		if (s_RendererData->Meshes.empty())
		{
			// Just to clear images & transition layouts
			Renderer::Submit([](Ref<CommandBuffer>& cmd)
			{
				EG_GPU_TIMING_SCOPED(cmd, "Render meshes");
				EG_CPU_TIMING_SCOPED("Renderer. Render meshes");

				cmd->BeginGraphics(s_RendererData->MeshPipeline);
				cmd->EndGraphics();
			});
			return;
		}

		Renderer::Submit([meshes = std::move(s_RendererData->Meshes), materials = std::move(s_RendererData->ShaderMaterials)](Ref<CommandBuffer>& cmd)
		{
			{
				EG_GPU_TIMING_SCOPED(cmd, "Render meshes");
				EG_CPU_TIMING_SCOPED("Renderer. Render meshes");

				const size_t materilBufferSize = s_RendererData->MaterialBuffer->GetSize();
				const size_t materialDataSize = materials.size() * sizeof(CPUMaterial);

				if (materialDataSize > materilBufferSize)
					s_RendererData->MaterialBuffer->Resize((materilBufferSize * 3) / 2);

				s_RendererData->MeshPipeline->SetBuffer(s_RendererData->MaterialBuffer, EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
				cmd->Write(s_RendererData->MaterialBuffer, materials.data(), materialDataSize, 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);

				struct VertexPushData
				{
					glm::mat4 Model = glm::mat4(1.f);
					uint32_t MaterialIndex = 0;
				} pushData;

				UpdateBuffers(cmd, meshes);

				uint32_t firstIndex = 0;
				uint32_t vertexOffset = 0;
				uint32_t meshIndex = 0;

				cmd->BeginGraphics(s_RendererData->MeshPipeline);
				for (auto& mesh : meshes)
				{
					const auto& vertices = mesh->StaticMesh->GetVertices();
					const auto& indices = mesh->StaticMesh->GetIndeces();
					size_t vertexSize = vertices.size() * sizeof(Vertex);
					size_t indexSize = indices.size() * sizeof(Index);

					pushData.Model = Math::ToTransformMatrix(mesh->GetWorldTransform());
					pushData.MaterialIndex = meshIndex;

					const uint32_t objectID = mesh->Parent.GetID();
					cmd->SetGraphicsRootConstants(&pushData, &objectID);
					cmd->DrawIndexed(s_RendererData->VertexBuffer, s_RendererData->IndexBuffer, (uint32_t)indices.size(), firstIndex, vertexOffset);
					firstIndex += (uint32_t)indices.size();
					vertexOffset += (uint32_t)vertices.size();
					meshIndex++;

					s_RendererData->Stats[s_RendererData->CurrentFrameIndex].DrawCalls++;
					s_RendererData->Stats[s_RendererData->CurrentFrameIndex].Vertices += (uint32_t)vertices.size();
					s_RendererData->Stats[s_RendererData->CurrentFrameIndex].Indeces += (uint32_t)indices.size();
				}
				cmd->EndGraphics();
			}

			ShadowPass(cmd, meshes);
		});
	}

	void Renderer::RenderSprites()
	{
		const auto& sprites = s_RendererData->Sprites;

		if (sprites.empty())
			return;

		Renderer2D::BeginScene(s_RendererData->CurrentFrameViewProj);
		for(auto& sprite : sprites)
			Renderer2D::DrawQuad(*sprite);
		Renderer2D::EndScene();
	}

	void Renderer::RenderBillboards()
	{
		if (s_RendererData->Miscellaneous.empty())
			return;

		EG_CPU_TIMING_SCOPED("Renderer. Render Billboards");

		static constexpr glm::vec4 quadVertexPosition[4] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f, 0.0f, 1.0f }, { -0.5f, 0.5f, 0.0f, 1.0f } };
		static constexpr glm::vec2 texCoords[4] = { {0.0f, 1.0f}, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f } };

		for (auto& misc : s_RendererData->Miscellaneous)
		{
			glm::mat4 transform = Math::ToTransformMatrix(misc.Transform);
			uint32_t textureIndex = (uint32_t)Renderer::AddTexture(*misc.Texture);

			for (int i = 0; i < 4; ++i)
			{
				auto& vertex = s_RendererData->BillboardData.Vertices.emplace_back();
				mat4 modelView = s_RendererData->CurrentFrameView * transform;
				
				// Remove rotation, apply scaling
				modelView[0] = vec4(misc.Transform.Scale3D.x, 0, 0, 0);
				modelView[1] = vec4(0, misc.Transform.Scale3D.y, 0, 0);
				modelView[2] = vec4(0, 0, misc.Transform.Scale3D.z, 0);

				vertex.Position = modelView * quadVertexPosition[i];
				vertex.TexCoord = texCoords[i];
				vertex.TextureIndex = textureIndex;
			}
		}

		Renderer::Submit([](Ref<CommandBuffer>& cmd)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Render billboards");
			EG_CPU_TIMING_SCOPED("Renderer::Submit. Render Billboards");


			// Reserving enough space to hold Vertex & Index data
			size_t currentVertexSize = s_RendererData->BillboardData.Vertices.size() * sizeof(BillboardVertex);
			size_t currentIndexSize = (s_RendererData->BillboardData.Vertices.size() / 4) * (sizeof(Index) * 6);

			if (currentVertexSize > s_RendererData->BillboardData.VertexBuffer->GetSize())
			{
				size_t newSize = glm::max(currentVertexSize, s_RendererData->BillboardData.VertexBuffer->GetSize() * 3 / 2);
				const size_t alignment = 4 * sizeof(BillboardVertex);
				newSize += alignment - (newSize % alignment);
				s_RendererData->BillboardData.VertexBuffer->Resize(newSize);
			}
			if (currentIndexSize > s_RendererData->BillboardData.IndexBuffer->GetSize())
			{
				size_t newSize = glm::max(currentVertexSize, s_RendererData->BillboardData.IndexBuffer->GetSize() * 3 / 2);
				const size_t alignment = 6 * sizeof(Index);
				newSize += alignment - (newSize % alignment);

				s_RendererData->BillboardData.IndexBuffer->Resize(newSize);
				UpdateIndexBuffer(cmd, s_RendererData->BillboardData.IndexBuffer);
			}

			uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
			auto& vb = s_RendererData->BillboardData.VertexBuffer;
			auto& ib = s_RendererData->BillboardData.IndexBuffer;
			cmd->Write(vb, s_RendererData->BillboardData.Vertices.data(), s_RendererData->BillboardData.Vertices.size() * sizeof(BillboardVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);

			const uint64_t texturesChangedFrame = Renderer::UsedTextureChangedFrame();
			const bool bUpdateTextures = Renderer::UsedTextureChanged() || (texturesChangedFrame >= s_RendererData->BillboardData.TexturesUpdatedFrame);
			s_RendererData->BillboardData.TexturesUpdatedFrame = texturesChangedFrame;

			if (bUpdateTextures)
				s_RendererData->BillboardData.Pipeline->SetImageSamplerArray(Renderer::GetUsedImages(), Renderer::GetUsedSamplers(), 0, 0);

			const uint32_t quadsCount = (uint32_t)(s_RendererData->BillboardData.Vertices.size() / 4);

			cmd->BeginGraphics(s_RendererData->BillboardData.Pipeline);
			cmd->SetGraphicsRootConstants(&s_RendererData->CurrentFrameProj, nullptr);
			cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
			cmd->EndGraphics();
		});
	}

	void Renderer::RegisterShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		s_ShaderDependencies[shader].Pipelines.push_back(pipeline);
	}

	void Renderer::RemoveShaderDependency(const Ref<Shader>& shader, const Ref<Pipeline>& pipeline)
	{
		auto& pipelines = s_ShaderDependencies[shader].Pipelines;
		auto it = std::find(pipelines.begin(), pipelines.end(), pipeline);
		if (it != pipelines.end())
			pipelines.erase(it);
	}

	void Renderer::OnShaderReloaded(const Ref<Shader>& shader)
	{
		auto it = s_ShaderDependencies.find(shader);
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
			s_RendererData->Meshes.push_back(&smComponent);
		}
	}

	void Renderer::DrawSprite(const SpriteComponent& sprite)
	{
		s_RendererData->Sprites.push_back( {&sprite} );
	}

	void Renderer::DrawBillboard(const Transform& transform, const Ref<Texture2D>& texture)
	{
		s_RendererData->Miscellaneous.push_back({ transform, &texture });
	}

	void Renderer::DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
	{
		s_RendererData->Lines.push_back( {start, end, color} );
	}

	void Renderer::DrawSkybox(const Ref<TextureCube>& cubemap)
	{
		s_RendererData->IBLTexture = cubemap;
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
		const float orthographicSize = 5.f;
		const float orthoLeft = -orthographicSize * aspectRatio;
		const float orthoRight = orthographicSize * aspectRatio;
		const float orthoBottom = -orthographicSize;
		const float orthoTop = orthographicSize;

		s_RendererData->OrthoProjection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, -200.f, 200.f);
		s_RendererData->OrthoProjection[1][1] *= -1;

		Application::Get().GetRenderContext()->WaitIdle();

		const auto& size = s_RendererData->ViewportSize;
		s_RendererData->ViewportSize = size;
		s_RendererData->ColorImage->Resize({ size, 1 });
		s_RendererData->GBufferImages.Resize({ size, 1 });
		s_RendererData->MeshPipeline->Resize(size.x, size.y);
		s_RendererData->PBRPipeline->Resize(size.x, size.y);
		s_RendererData->SkyboxPipeline->Resize(size.x, size.y);
		s_RendererData->BillboardData.Pipeline->Resize(size.x, size.y);
		Renderer2D::OnResized(size);

	}

	float Renderer::GetGamma()
	{
		return s_RendererData->Gamma;
	}

	void Renderer::SetGamma(float gamma)
	{
		s_RendererData->Gamma = gamma;
		SetPhotoLinearTonemappingParams(s_RendererData->PhotoLinearParams);
	}

	float& Renderer::Exposure()
	{
		return s_RendererData->Exposure;
	}

	TonemappingMethod& Renderer::TonemappingMethod()
	{
		return s_RendererData->TonemappingMethod;
	}

	void Renderer::SetPhotoLinearTonemappingParams(const PhotoLinearTonemappingParams& params)
	{
		s_RendererData->PhotoLinearParams = params;
		// H = q L t / N^2
			//
			// where:
			//  q has a typical value is q = 0.65
			//  L is the luminance of the scene in candela per m^2 (sensitivity)
			//  t is the exposure time in seconds (exposure)
			//  N is the aperture f-number (fstop)
		s_RendererData->PhotoLinearScale = 0.65f * params.ExposureTime * params.Sensetivity /
			(params.FStop * params.FStop) * 10.f /
			pow(118.f / 255.f, s_RendererData->Gamma);
	}

	FilmicTonemappingParams& Renderer::FilmicTonemappingParams()
	{
		return s_RendererData->FilmicParams;
	}

	bool Renderer::IsVisualizingCascades()
	{
		return s_RendererData->bVisualizingCascades;
	}

	void Renderer::SetVisualizeCascades(bool bVisualize)
	{
		s_RendererData->bVisualizingCascades = bVisualize;
		auto& defines = s_RendererData->PBRDefines;
		auto& shader = s_RendererData->PBRFragShader;

		bool bUpdate = false;
		if (bVisualize)
		{
			defines["EG_ENABLE_CSM_VISUALIZATION"] = "";
			bUpdate = true;
		}
		else
		{
			auto it = defines.find("EG_ENABLE_CSM_VISUALIZATION");
			if (it != defines.end())
			{
				defines.erase(it);
				bUpdate = true;
			}
		}
		if (bUpdate)
		{
			shader->SetDefines(defines);
			shader->Reload();
		}
	}

	bool Renderer::IsSoftShadowsEnabled()
	{
		return s_RendererData->bSoftShadows;
	}

	void Renderer::SetSoftShadowsEnabled(bool bEnable)
	{
		s_RendererData->bSoftShadows = bEnable;
		auto& defines = s_RendererData->PBRDefines;
		auto& shader = s_RendererData->PBRFragShader;

		bool bUpdate = false;
		if (bEnable)
		{
			CreateShadowMapDistribution(EG_SM_DISTRIBUTION_TEXTURE_SIZE, EG_SM_DISTRIBUTION_FILTER_SIZE);
			defines["EG_SOFT_SHADOWS"] = "";
			bUpdate = true;
		}
		else
		{
			auto it = defines.find("EG_SOFT_SHADOWS");
			if (it != defines.end())
			{
				s_RendererData->ShadowMapDistribution.reset();
				defines.erase(it);
				bUpdate = true;
			}
		}
		if (bUpdate)
		{
			shader->SetDefines(defines);
			shader->Reload();
		}
	}

	Ref<Image>& Renderer::GetFinalImage()
	{
		return s_RendererData->FinalImage;
	}

	GBuffers& Renderer::GetGBuffers()
	{
		return s_RendererData->GBufferImages;
	}

	RenderCommandQueue& Renderer::GetResourceReleaseQueue(uint32_t index)
	{
		return s_ResourceFreeQueue[index];
	}

	Ref<CommandBuffer>& Renderer::GetCurrentFrameCommandBuffer()
	{
		return s_RendererData->CommandBuffers[s_RendererData->CurrentFrameIndex];
	}

	const RendererCapabilities& Renderer::GetCapabilities()
	{
		return Application::Get().GetRenderContext()->GetCapabilities();
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

	Ref<PipelineGraphics>& Renderer::GetIBLPipeline()
	{
		return s_RendererData->IBLPipeline;
	}

	Ref<PipelineGraphics>& Renderer::GetIrradiancePipeline()
	{
		return s_RendererData->IrradiancePipeline;
	}

	Ref<PipelineGraphics>& Renderer::GetPrefilterPipeline()
	{
		return s_RendererData->PrefilterPipeline;
	}

	Ref<PipelineGraphics>& Renderer::GetBRDFLUTPipeline()
	{
		return s_RendererData->BRDFLUTPipeline;
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

	const GPUTimingsMap& Renderer::GetTimings()
	{
		return s_RendererData->GPUTimings;
	}

#ifdef EG_GPU_TIMINGS
	void Renderer::RegisterGPUTiming(Ref<RHIGPUTiming>& timing, std::string_view name)
	{
		s_RendererData->RHIGPUTimings[name] = timing;
	}

	const std::unordered_map<std::string_view, Ref<RHIGPUTiming>>& Renderer::GetRHITimings()
	{
		return s_RendererData->RHIGPUTimings;
	}
#endif

	Renderer::Statistics Renderer::GetStats()
	{
		uint32_t index = s_RendererData->CurrentFrameIndex;
		index = index == 0 ? RendererConfig::FramesInFlight - 2 : index - 1;

		return s_RendererData->Stats[index]; // Returns stats of the prev frame because current frame stats are not ready yet
	}

	void GBuffers::Init(const glm::uvec3& size)
	{
		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = size;
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled;
		s_RendererData->GBufferImages.Depth = Image::Create(depthSpecs, "GBuffer_Depth");

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = size;
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->GBufferImages.Albedo = Image::Create(colorSpecs, "GBuffer_Albedo");

		ImageSpecifications normalSpecs;
		normalSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		normalSpecs.Layout = ImageLayoutType::RenderTarget;
		normalSpecs.Size = size;
		normalSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->GBufferImages.GeometryNormal = Image::Create(normalSpecs, "GBuffer_GeometryNormal");
		s_RendererData->GBufferImages.ShadingNormal = Image::Create(normalSpecs, "GBuffer_ShadingNormal");

		ImageSpecifications materialSpecs;
		materialSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		materialSpecs.Layout = ImageLayoutType::RenderTarget;
		materialSpecs.Size = size;
		materialSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_RendererData->GBufferImages.MaterialData = Image::Create(materialSpecs, "GBuffer_MaterialData");

		ImageSpecifications objectIDSpecs;
		objectIDSpecs.Format = ImageFormat::R32_SInt;
		objectIDSpecs.Layout = ImageLayoutType::RenderTarget;
		objectIDSpecs.Size = size;
		objectIDSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;
		s_RendererData->GBufferImages.ObjectID = Image::Create(objectIDSpecs, "GBuffer_ObjectID");
	}
}
