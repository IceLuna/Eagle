#include "egpch.h"
#include "OutlineTask.h"
#include "RenderMeshesTask.h"
#include "RenderSkeletalMeshesTask.h"
#include "RenderBillboardsTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/MaterialSystem.h"
#include "Eagle/Renderer/TextureSystem.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/Image.h"
#include "Eagle/Renderer/VidWrappers/Framebuffer.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/outline/outline_common.h"

namespace Eagle
{
	// Skeletal meshes fetch vertices manually, so VB & IVB are null for their draw calls
	static Ref<Buffer> s_NullBuffer = nullptr;

	static OutlineStyle ToGPUStyle(const OutlineSettings& settings)
	{
		OutlineStyle style;
		style.Color = glm::packUnorm4x8(settings.Color);
		style.Thickness = glm::clamp(settings.Thickness, 0.f, OutlineSettings::MaxThickness);

		switch (settings.Mode)
		{
			case OutlineMode::VisibleOnly:
				style.VisibleOpacity = 1.f;
				style.HiddenOpacity = 0.f;
				break;
			case OutlineMode::HiddenOnly:
				style.VisibleOpacity = 0.f;
				style.HiddenOpacity = 1.f;
				break;
			case OutlineMode::DimHidden:
				style.VisibleOpacity = 1.f;
				style.HiddenOpacity = glm::clamp(settings.HiddenOpacity, 0.f, 1.f);
				break;
			case OutlineMode::Always:
			default:
				style.VisibleOpacity = 1.f;
				style.HiddenOpacity = 1.f;
				break;
		}
		return style;
	}

	template <typename InstanceDataType, typename GetStyleFunc>
	static void GatherMeshes(const MeshesDrawLists& drawLists, const std::vector<InstanceDataType>& instances, bool bSkeletal, const GetStyleFunc& getStyleID,
		std::vector<OutlineTask::MeshDraw>& outDraws, std::vector<OutlineTask::MeshDraw>& outMaskedDraws)
	{
		auto process = [&](const MeshDrawDataInfo& info, bool bDoubleSided, std::vector<OutlineTask::MeshDraw>& out)
		{
			const uint32_t instancesCount = (uint32_t)instances.size();
			for (const auto& mesh : info.DrawData)
			{
				for (const auto& matData : mesh.PerMaterialData)
				{
					for (uint32_t i = 0; i < matData.InstanceCount; ++i)
					{
						const uint32_t instanceIndex = matData.FirstInstance + i;
						if (instanceIndex >= instancesCount)
							break;

						const uint32_t styleID = getStyleID(instances[instanceIndex].ObjectID);
						if (styleID == 0)
							continue;

						auto& draw = out.emplace_back();
						draw.IndexCount = matData.IndexCount;
						draw.FirstIndex = matData.FirstIndex;
						draw.VertexOffset = bSkeletal ? (mesh.SkinnedVertexOffset + mesh.VerticesCount * i) : mesh.VertexOffset;
						draw.InstanceIndex = instanceIndex;
						draw.StyleID = styleID;
						draw.bDoubleSided = bDoubleSided;
					}
				}
			}
		};

		process(drawLists.SingleSided.Opaque, false, outDraws);
		process(drawLists.SingleSided.Translucent, false, outDraws);
		process(drawLists.SingleSided.Masked, false, outMaskedDraws);
		process(drawLists.DoubleSided.Opaque, true, outDraws);
		process(drawLists.DoubleSided.Translucent, true, outDraws);
		process(drawLists.DoubleSided.Masked, true, outMaskedDraws);
	}

	// Collects quads (sprites, glyphs, billboards) that need to be outlined.
	// Consecutive quads with the same style are merged into a single draw (for example, all glyphs of a text)
	template <typename VertexType, typename GetStyleFunc>
	static void GatherQuads(const std::vector<VertexType>& vertices, const Ref<Buffer>& vb, const Ref<Buffer>& ib, bool bDoubleSided, const GetStyleFunc& getStyleID,
		std::vector<OutlineTask::QuadsDraw>& outDraws)
	{
		if (!vb || !ib)
			return;

		const uint32_t quadsCount = (uint32_t)(vertices.size() / 4);
		for (uint32_t quad = 0; quad < quadsCount; ++quad)
		{
			const uint32_t styleID = getStyleID((uint32_t)vertices[quad * 4].EntityID);
			if (styleID == 0)
				continue;

			if (!outDraws.empty())
			{
				auto& last = outDraws.back();
				if (last.VertexBuffer == vb && last.StyleID == styleID && (last.FirstQuad + last.QuadsCount) == quad)
				{
					++last.QuadsCount;
					continue;
				}
			}

			auto& draw = outDraws.emplace_back();
			draw.VertexBuffer = vb;
			draw.IndexBuffer = ib;
			draw.FirstQuad = quad;
			draw.QuadsCount = 1;
			draw.StyleID = styleID;
			draw.bDoubleSided = bDoubleSided;
		}
	}

	template <typename GeometryDataType, typename GetStyleFunc>
	static void GatherQuads(const GeometryDataType& data, bool bDoubleSided, const GetStyleFunc& getStyleID, std::vector<OutlineTask::QuadsDraw>& outDraws)
	{
		GatherQuads(data.QuadVertices, data.VertexBuffer, data.IndexBuffer, bDoubleSided, getStyleID, outDraws);
	}

	template <typename BlendModeGeomType, typename GetStyleFunc>
	static void GatherBlendModeQuads(const BlendModeGeomType& data, bool bDoubleSided, const GetStyleFunc& getStyleID, std::vector<OutlineTask::QuadsDraw>& outDraws)
	{
		GatherQuads(data.ShadowCastingQuads, bDoubleSided, getStyleID, outDraws);
		GatherQuads(data.NonShadowQuads, bDoubleSided, getStyleID, outDraws);
	}

	OutlineTask::OutlineTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipelines();
	}

	void OutlineTask::SetOutlines(ankerl::unordered_dense::map<uint32_t, OutlineSettings>&& outlines)
	{
		m_ObjectStyles.clear();
		m_Styles.clear();
		m_MaxRadius = 0;

		m_ObjectStyles.reserve(outlines.size());
		uint32_t skippedObjects = 0;
		for (const auto& [objectID, settings] : outlines)
		{
			const OutlineStyle gpuStyle = ToGPUStyle(settings);
			const bool bInvisible = settings.Color.a <= 0.f || gpuStyle.Thickness <= 0.f || (gpuStyle.VisibleOpacity <= 0.f && gpuStyle.HiddenOpacity <= 0.f);
			if (bInvisible)
				continue;

			auto it = std::find(m_Styles.begin(), m_Styles.end(), settings);
			uint32_t styleIndex = 0;
			if (it == m_Styles.end())
			{
				if (m_Styles.size() >= EG_OUTLINE_MAX_STYLES)
				{
					++skippedObjects;
					continue;
				}

				styleIndex = (uint32_t)m_Styles.size();
				m_Styles.push_back(settings);
				m_MaxRadius = glm::max(m_MaxRadius, (uint32_t)glm::ceil(gpuStyle.Thickness));
			}
			else
			{
				styleIndex = (uint32_t)(it - m_Styles.begin());
			}

			m_ObjectStyles.emplace(objectID, styleIndex + 1);
		}

		if (skippedObjects > 0)
			EG_RENDERER_WARN("Too many unique outline styles (the limit is {}). {} objects won't be outlined", EG_OUTLINE_MAX_STYLES, skippedObjects);

		bStylesDirty = true;
		bDrawListsDirty = true;
	}

	uint32_t OutlineTask::GetStyleID(uint32_t objectID) const
	{
		auto it = m_ObjectStyles.find(objectID);
		return it != m_ObjectStyles.end() ? it->second : 0;
	}

	void OutlineTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_ObjectStyles.empty())
		{
			ReleaseResources();
			return;
		}

		EG_GPU_TIMING_SCOPED(cmd, "Outlines");
		EG_CPU_TIMING_SCOPED("Outlines");

		if (!m_Mask)
			InitResources();

		const uint64_t geometryVersion = m_Renderer.GetGeometryVersion();
		if (bDrawListsDirty || geometryVersion != m_GeometryVersion)
		{
			BuildDrawLists();
			m_GeometryVersion = geometryVersion;
			bDrawListsDirty = false;
		}

		BuildBillboardDrawList();

		if (m_DrawLists.IsEmpty() && m_BillboardDraws.empty())
			return;

		UploadStyles(cmd);

		// Mask pass. Outlined objects are rendered into an R16_UInt mask using their own depth buffer.
		// Each pixel stores the outline style and whether the object is occluded by the scene there (tested against the GBuffer depth).
		// Opacity-masked materials, font glyphs and billboard textures are alpha-tested, so the outline follows the visible shape of an object, not its geometry.
		// Translucent materials are outlined by their geometry.
		RenderMask(cmd);

		// Composite pass. A compute shader dilates the mask and applies the outlines.
		Composite(cmd);
	}

	void OutlineTask::OnResize(glm::uvec2 size)
	{
		if (!m_Mask)
			return;

		m_Mask->Resize({ size, 1 });
		m_MaskDepth->Resize({ size, 1 });

		// No need to call "Resize" for pipelines, the updated size comes from the framebuffer
		m_MaskFramebuffer = Framebuffer::Create({ m_Mask, m_MaskDepth }, size, m_MeshPipeline.Pipeline->GetRenderPassHandle());
	}

	void OutlineTask::InitResources()
	{
		const glm::uvec2 size = m_Renderer.GetViewportSize();

		ImageSpecifications maskSpecs;
		maskSpecs.Format = ImageFormat::R16_UInt;
		maskSpecs.Layout = ImageLayoutType::RenderTarget;
		maskSpecs.Size = { size, 1u };
		maskSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferDst;
		m_Mask = Image::Create(maskSpecs, "Outline_Mask");

		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = { size, 1u };
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment | ImageUsage::TransferDst;
		m_MaskDepth = Image::Create(depthSpecs, "Outline_MaskDepth");

		m_MaskFramebuffer = Framebuffer::Create({ m_Mask, m_MaskDepth }, size, m_MeshPipeline.Pipeline->GetRenderPassHandle());

		BufferSpecifications stylesSpecs;
		stylesSpecs.Size = sizeof(OutlineStyle) * 8;
		stylesSpecs.Layout = BufferLayoutType::StorageBuffer;
		stylesSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
		m_StylesBuffer = Buffer::Create(stylesSpecs, "Outline_Styles");
	}

	void OutlineTask::ReleaseResources()
	{
		m_MaskFramebuffer.reset();
		m_Mask.reset();
		m_MaskDepth.reset();
		m_StylesBuffer.reset();
	}

	void OutlineTask::InitPipelines()
	{
		const ShaderDefines materials = { { "EG_MATERIALS_REQUIRED", "" } };
		const Ref<Shader> maskFS = Shader::Create("outline/outline_mask.frag", ShaderType::Fragment);
		const Ref<Shader> maskedMaskFS = Shader::Create("outline/outline_mask.frag", ShaderType::Fragment, materials);
		const Ref<Shader> textFS = Shader::Create("outline/outline_mask.frag", ShaderType::Fragment, { { "EG_TEXT", "" } });
		const Ref<Shader> maskedTextFS = Shader::Create("outline/outline_mask.frag", ShaderType::Fragment, { { "EG_TEXT", "" }, { "EG_MATERIALS_REQUIRED", "" } });
		const Ref<Shader> billboardFS = Shader::Create("outline/outline_mask.frag", ShaderType::Fragment, { { "EG_BILLBOARD", "" } });

		// Dummy attachments are used to avoid creating real resources if no outlines are used.
		// And this way, we can still create pipelines beforehand.
		ImageSpecifications placeholderSpecs;
		placeholderSpecs.Format = ImageFormat::R16_UInt;
		placeholderSpecs.Layout = ImageLayoutType::RenderTarget;
		placeholderSpecs.Size = { 1u, 1u, 1u };
		placeholderSpecs.Usage = ImageUsage::ColorAttachment;
		const Ref<Image> maskPlaceholder = Image::Create(placeholderSpecs, "Outline_MaskPlaceholder");
		const Ref<Image>& depthPlaceholder = RenderManager::GetDummyDepthImage();

		auto create = [&maskPlaceholder, &depthPlaceholder](OutlinePipeline& outPipeline, const Ref<Shader>& vs, const Ref<Shader>& fs, const std::vector<PipelineGraphicsState::VertexInputAttribute>& perInstanceAttribs = {})
		{
			ColorAttachment maskAttachment;
			maskAttachment.Image = maskPlaceholder;
			maskAttachment.InitialLayout = ImageLayoutType::RenderTarget;
			maskAttachment.FinalLayout = ImageLayoutType::RenderTarget;
			maskAttachment.ClearOperation = ClearOperation::Load;

			DepthStencilAttachment depthAttachment;
			depthAttachment.Image = depthPlaceholder;
			depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
			depthAttachment.ClearOperation = ClearOperation::Load;
			depthAttachment.bWriteDepth = true;
			depthAttachment.DepthCompareOp = CompareOperation::Greater;

			PipelineGraphicsState state;
			state.ColorAttachments.push_back(maskAttachment);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Dynamic;
			state.VertexShader = vs;
			state.FragmentShader = fs;
			state.PerInstanceAttribs = perInstanceAttribs;
			outPipeline.Pipeline = PipelineGraphics::Create(state);
		};

		// Static meshes
		create(m_MeshPipeline, Shader::Create("outline/outline_mesh.vert", ShaderType::Vertex), maskFS, RenderMeshesTask::PerInstanceAttribs);
		create(m_MaskedMeshPipeline, Shader::Create("outline/outline_mesh.vert", ShaderType::Vertex, materials), maskedMaskFS, RenderMeshesTask::PerInstanceAttribs);

		// Skeletal meshes
		create(m_SkeletalMeshPipeline, Shader::Create("outline/outline_skeletal_mesh.vert", ShaderType::Vertex), maskFS, RenderSkeletalMeshesTask::PerInstanceAttribs);
		create(m_MaskedSkeletalMeshPipeline, Shader::Create("outline/outline_skeletal_mesh.vert", ShaderType::Vertex, materials), maskedMaskFS, RenderSkeletalMeshesTask::PerInstanceAttribs);

		// Quads (sprite, texts, billboards)
		auto quadVS = [](const char* quadType, bool bMaterials = false)
		{
			ShaderDefines defines = { { quadType, "" } };
			if (bMaterials)
				defines["EG_MATERIALS_REQUIRED"] = "";
			return Shader::Create("outline/outline_quad.vert", ShaderType::Vertex, defines);
		};

		// Sprites
		create(m_SpritePipeline, quadVS("EG_SPRITE"), maskFS);
		create(m_MaskedSpritePipeline, quadVS("EG_SPRITE", true), maskedMaskFS);

		// Texts
		create(m_LitTextPipeline, quadVS("EG_LIT_TEXT"), textFS);
		create(m_MaskedLitTextPipeline, quadVS("EG_LIT_TEXT", true), maskedTextFS);
		create(m_UnlitTextPipeline, quadVS("EG_UNLIT_TEXT"), textFS);

		// Billboards
		create(m_BillboardPipeline, quadVS("EG_BILLBOARD"), billboardFS);

		PipelineComputeState compositeState;
		compositeState.ComputeShader = Shader::Create("outline/outline_composite.comp", ShaderType::Compute);
		m_CompositePipeline = PipelineCompute::Create(compositeState);
	}

	void OutlineTask::BuildDrawLists()
	{
		EG_CPU_TIMING_SCOPED("Outlines. Build draw lists");

		m_DrawLists.Clear();
		auto getStyleID = [this](uint32_t objectID) { return GetStyleID(objectID); };

		// Static meshes
		{
			const auto& instances = m_Renderer.GetStaticMeshesBuffers().InstanceVertices;
			GatherMeshes(m_Renderer.GetStaticMeshesDrawData(), instances, false, getStyleID, m_DrawLists.Meshes, m_DrawLists.MaskedMeshes);
		}

		// Skeletal meshes
		{
			const auto& instances = m_Renderer.GetSkeletalMeshesBuffers().InstanceVertices;
			GatherMeshes(m_Renderer.GetSkeletalMeshesDrawData(), instances, true, getStyleID, m_DrawLists.SkeletalMeshes, m_DrawLists.MaskedSkeletalMeshes);
		}

		// Sprites
		for (const bool bDoubleSided : { false, true })
		{
			const auto& sprites = bDoubleSided ? m_Renderer.GetDoubleSidedSpritesRenderData() : m_Renderer.GetSingleSidedSpritesRenderData();
			GatherBlendModeQuads(sprites.Opaque, bDoubleSided, getStyleID, m_DrawLists.Sprites);
			GatherBlendModeQuads(sprites.Translucent, bDoubleSided, getStyleID, m_DrawLists.Sprites);
			GatherBlendModeQuads(sprites.Masked, bDoubleSided, getStyleID, m_DrawLists.MaskedSprites);
		}

		// Texts
		for (const bool bDoubleSided : { false, true })
		{
			const auto& litTexts = bDoubleSided ? m_Renderer.GetDoubleSidedTextsRenderData() : m_Renderer.GetSingleSidedTextsRenderData();
			GatherBlendModeQuads(litTexts.Opaque, bDoubleSided, getStyleID, m_DrawLists.LitTexts);
			GatherBlendModeQuads(litTexts.Translucent, bDoubleSided, getStyleID, m_DrawLists.LitTexts);
			GatherBlendModeQuads(litTexts.Masked, bDoubleSided, getStyleID, m_DrawLists.MaskedLitTexts);

			const auto& unlitTexts = bDoubleSided ? m_Renderer.GetDoubleSidedUnlitTextsRenderData() : m_Renderer.GetSingleSidedUnlitTextsRenderData();
			GatherBlendModeQuads(unlitTexts.Opaque, bDoubleSided, getStyleID, m_DrawLists.UnlitTexts);
			GatherBlendModeQuads(unlitTexts.Translucent, bDoubleSided, getStyleID, m_DrawLists.UnlitTexts);
		}
	}

	void OutlineTask::BuildBillboardDrawList()
	{
		m_BillboardDraws.clear();
		auto getStyleID = [this](uint32_t objectID) { return GetStyleID(objectID); };
		constexpr bool bDoubleSided = true; // Billboards always face the camera
		GatherQuads(m_Renderer.GetBillboardsVertices(), m_Renderer.GetBillboardsVertexBuffer(), m_Renderer.GetBillboardsIndexBuffer(), bDoubleSided, getStyleID, m_BillboardDraws);
	}

	void OutlineTask::UploadStyles(const Ref<CommandBuffer>& cmd)
	{
		if (!bStylesDirty)
			return;

		bStylesDirty = false;
		if (m_Styles.empty())
			return;

		std::vector<OutlineStyle> gpuStyles;
		gpuStyles.reserve(m_Styles.size());
		for (const auto& style : m_Styles)
			gpuStyles.push_back(ToGPUStyle(style));

		const size_t dataSize = gpuStyles.size() * sizeof(OutlineStyle);
		if (dataSize > m_StylesBuffer->GetSize())
			m_StylesBuffer->Resize((dataSize * 3) / 2);

		cmd->Write(m_StylesBuffer, gpuStyles.data(), dataSize, 0, m_StylesBuffer->GetLayout(), BufferLayoutType::StorageBuffer);
	}

	void OutlineTask::BindTextures(OutlinePipeline& pipeline) const
	{
		const uint32_t frameIndex = RenderManager::GetCurrentFrameIndex();
		const uint64_t texturesChangedFrame = TextureSystem::GetUpdatedFrameNumber();
		if (texturesChangedFrame >= pipeline.TexturesUpdatedFrames[frameIndex])
		{
			pipeline.Pipeline->SetImageSamplerArray(TextureSystem::GetImages(), TextureSystem::GetSamplers(), EG_TEXTURES_SET, EG_BINDING_TEXTURES);
			pipeline.TexturesUpdatedFrames[frameIndex] = texturesChangedFrame + 1;
		}
	}

	void OutlineTask::BindMaterials(OutlinePipeline& pipeline) const
	{
		BindTextures(pipeline);
		pipeline.Pipeline->SetBuffer(MaterialSystem::GetMaterialsBuffer(), EG_PERSISTENT_SET, EG_BINDING_MATERIALS);
		pipeline.Pipeline->SetBuffer(MaterialSystem::GetMaterialsRawBuffer(), EG_PERSISTENT_SET, EG_BINDING_RAW_MATERIALS);
	}

	void OutlineTask::DrawMeshes(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<MeshDraw>& draws, bool bSkeletal) const
	{
		if (draws.empty())
			return;

		const auto& vb = bSkeletal ? s_NullBuffer : m_Renderer.GetStaticMeshesBuffers().VertexBuffer;
		const auto& ivb = bSkeletal ? s_NullBuffer : m_Renderer.GetStaticMeshesBuffers().InstanceBuffer;
		const auto& ib = bSkeletal ? m_Renderer.GetSkeletalMeshesBuffers().IndexBuffer : m_Renderer.GetStaticMeshesBuffers().IndexBuffer;
		auto& stats = m_Renderer.GetStats();

		OutlinePushData pushData;
		pushData.ViewProj = m_Renderer.GetCameraMatrices().ViewProjUnjittered;

		cmd->BeginGraphics(pipeline, m_MaskFramebuffer);
		for (const auto& draw : draws)
		{
			pushData.StyleID = draw.StyleID;
			pushData.VertexOffset = bSkeletal ? draw.VertexOffset : 0u;
			const int32_t vertexOffset = bSkeletal ? 0 : (int32_t)draw.VertexOffset; // Skeletal vertices are fetched manually

			cmd->SetGraphicsCullMode(draw.bDoubleSided ? CullMode::None : CullMode::Back);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			cmd->DrawIndexedInstanced(vb, ib, draw.IndexCount, draw.FirstIndex, vertexOffset, 1u, draw.InstanceIndex, ivb);
			++stats.DrawCalls;
		}
		cmd->EndGraphics();
	}

	void OutlineTask::DrawQuads(const Ref<CommandBuffer>& cmd, const Ref<PipelineGraphics>& pipeline, const std::vector<QuadsDraw>& draws, const glm::mat4& viewProj) const
	{
		if (draws.empty())
			return;

		auto& stats = m_Renderer.GetStats();

		OutlinePushData pushData;
		pushData.ViewProj = viewProj;

		cmd->BeginGraphics(pipeline, m_MaskFramebuffer);
		for (const auto& draw : draws)
		{
			pushData.StyleID = draw.StyleID;

			cmd->SetGraphicsCullMode(draw.bDoubleSided ? CullMode::None : CullMode::Back);
			cmd->SetGraphicsRootConstants(&pushData, nullptr);
			cmd->DrawIndexed(draw.VertexBuffer, draw.IndexBuffer, draw.QuadsCount * 6u, draw.FirstQuad * 6u, 0u);
			++stats.DrawCalls;
		}
		cmd->EndGraphics();
	}

	void OutlineTask::RenderMask(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Outlines. Mask");
		EG_CPU_TIMING_SCOPED("Outlines. Mask");

		const auto& sceneDepth = m_Renderer.GetGBuffer().Depth;
		const ImageLayout sceneDepthLayout = sceneDepth->GetLayout();
		cmd->TransitionLayout(sceneDepth, sceneDepthLayout, ImageReadAccess::PixelShaderRead);

		cmd->ClearColorImage(m_Mask, glm::vec4(0.f), m_Mask->GetLayout(), ImageLayoutType::RenderTarget);
		cmd->ClearDepthStencilImage(m_MaskDepth, 0.f, 0, m_MaskDepth->GetLayout(), ImageLayoutType::DepthStencilWrite);

		const auto& viewProj = m_Renderer.GetCameraMatrices().ViewProjUnjittered;
		const auto& draws = m_DrawLists;

		// Static meshes
		{
			const auto& transforms = m_Renderer.GetMeshTransformsBuffer();
			if (!draws.Meshes.empty())
			{
				auto& pipeline = m_MeshPipeline.Pipeline;
				pipeline->SetBuffer(transforms, 0, 0);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
				DrawMeshes(cmd, pipeline, draws.Meshes, false);
			}
			if (!draws.MaskedMeshes.empty())
			{
				BindMaterials(m_MaskedMeshPipeline);
				auto& pipeline = m_MaskedMeshPipeline.Pipeline;
				pipeline->SetBuffer(transforms, 1, 0);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 3, 0);
				DrawMeshes(cmd, pipeline, draws.MaskedMeshes, false);
			}
		}

		// Skeletal meshes
		{
			const auto& skinnedVertices = m_Renderer.GetSkinnedVertices();
			if (!draws.SkeletalMeshes.empty())
			{
				auto& pipeline = m_SkeletalMeshPipeline.Pipeline;
				pipeline->SetBuffer(skinnedVertices, 0, 0);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
				DrawMeshes(cmd, pipeline, draws.SkeletalMeshes, true);
			}
			if (!draws.MaskedSkeletalMeshes.empty())
			{
				BindMaterials(m_MaskedSkeletalMeshPipeline);
				auto& pipeline = m_MaskedSkeletalMeshPipeline.Pipeline;
				pipeline->SetBuffer(skinnedVertices, 1, 0);
				pipeline->SetBuffer(m_Renderer.GetSkeletalMeshesBuffers().InstanceBuffer, 1, 1);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 3, 0);
				DrawMeshes(cmd, pipeline, draws.MaskedSkeletalMeshes, true);
			}
		}

		// Sprites
		{
			const auto& transforms = m_Renderer.GetSpritesTransformsBuffer();
			if (!draws.Sprites.empty())
			{
				auto& pipeline = m_SpritePipeline.Pipeline;
				pipeline->SetBuffer(transforms, 0, 0);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
				DrawQuads(cmd, pipeline, draws.Sprites, viewProj);
			}
			if (!draws.MaskedSprites.empty())
			{
				BindMaterials(m_MaskedSpritePipeline);
				auto& pipeline = m_MaskedSpritePipeline.Pipeline;
				pipeline->SetBuffer(transforms, 1, 0);
				pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 3, 0);
				DrawQuads(cmd, pipeline, draws.MaskedSprites, viewProj);
			}
		}

		// Texts
		{
			const auto& transforms = m_Renderer.GetTextsTransformsBuffer();
			const auto& atlases = m_Renderer.GetAtlases();
			if (!atlases.empty())
			{
				if (!draws.LitTexts.empty())
				{
					auto& pipeline = m_LitTextPipeline.Pipeline;
					pipeline->SetBuffer(transforms, 0, 0);
					pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
					pipeline->SetTextureArray(atlases, 1, 1);
					DrawQuads(cmd, pipeline, draws.LitTexts, viewProj);
				}
				if (!draws.MaskedLitTexts.empty())
				{
					BindMaterials(m_MaskedLitTextPipeline);
					auto& pipeline = m_MaskedLitTextPipeline.Pipeline;
					pipeline->SetBuffer(transforms, 1, 0);
					pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 3, 0);
					pipeline->SetTextureArray(atlases, 3, 1);
					DrawQuads(cmd, pipeline, draws.MaskedLitTexts, viewProj);
				}
				if (!draws.UnlitTexts.empty())
				{
					auto& pipeline = m_UnlitTextPipeline.Pipeline;
					pipeline->SetBuffer(transforms, 0, 0);
					pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
					pipeline->SetTextureArray(atlases, 1, 1);
					DrawQuads(cmd, pipeline, draws.UnlitTexts, viewProj);
				}
			}
		}

		// Billboards
		if (!m_BillboardDraws.empty())
		{
			BindTextures(m_BillboardPipeline);
			auto& pipeline = m_BillboardPipeline.Pipeline;
			pipeline->SetImageSampler(sceneDepth, Sampler::PointSampler, 1, 0);
			DrawQuads(cmd, pipeline, m_BillboardDraws, m_Renderer.GetCameraMatrices().ProjUnjittered);
		}

		cmd->TransitionLayout(sceneDepth, ImageReadAccess::PixelShaderRead, sceneDepthLayout);
		cmd->TransitionLayout(m_Mask, m_Mask->GetLayout(), ImageReadAccess::NonPixelShaderRead);
	}

	void OutlineTask::Composite(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Outlines. Composite");
		EG_CPU_TIMING_SCOPED("Outlines. Composite");

		const auto& output = m_Renderer.GetOutput();
		const glm::uvec2 size = glm::min(glm::uvec2(output->GetSize()), glm::uvec2(m_Mask->GetSize()));

		const ImageLayout outputLayout = output->GetLayout();
		cmd->TransitionLayout(output, outputLayout, ImageLayoutType::StorageImage);

		m_CompositePipeline->SetImageSampler(m_Mask, Sampler::PointSampler, 0, 0);
		m_CompositePipeline->SetImage(output, 0, 1);
		m_CompositePipeline->SetBuffer(m_StylesBuffer, 0, 2);

		struct PushData
		{
			glm::ivec2 Size = glm::ivec2(0);
			uint32_t Radius = 0;
		} pushData;
		pushData.Size = size;
		pushData.Radius = m_MaxRadius;

		const glm::uvec2 numGroups = CalcNumGroups(size, m_CompositePipeline->GetWorkGroupSize());
		cmd->Dispatch(m_CompositePipeline, numGroups.x, numGroups.y, 1, &pushData);
		++m_Renderer.GetStats().Dispatches;

		cmd->TransitionLayout(output, ImageLayoutType::StorageImage, outputLayout);
	}

	void OutlineTask::DrawLists::Clear()
	{
		Meshes.clear();
		MaskedMeshes.clear();
		SkeletalMeshes.clear();
		MaskedSkeletalMeshes.clear();
		Sprites.clear();
		MaskedSprites.clear();
		LitTexts.clear();
		MaskedLitTexts.clear();
		UnlitTexts.clear();
	}

	bool OutlineTask::DrawLists::IsEmpty() const
	{
		return Meshes.empty() && MaskedMeshes.empty() &&
			SkeletalMeshes.empty() && MaskedSkeletalMeshes.empty() &&
			Sprites.empty() && MaskedSprites.empty() &&
			LitTexts.empty() && MaskedLitTexts.empty() && UnlitTexts.empty();
	}
}
