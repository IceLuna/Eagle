#include "egpch.h"
#include "RenderText2DTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Classes/Font.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

namespace Eagle
{
	template<typename MapType>
	static uint32_t GetFontAtlasIndex(MapType& atlases, const Ref<Texture2D>& atlas)
	{
		if (auto it = atlases.find(atlas); it != atlases.end())
			return it->second;

		if (atlases.size() == RendererConfig::MaxTextures)
		{
			EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", RendererConfig::MaxTextures);
			return 0;
		}

		const uint32_t index = (uint32_t)atlases.size();
		atlases.emplace(atlas, index);
		return index;
	}

	static void UploadIndexBuffer(const Ref<CommandBuffer>& cmd, Ref<Buffer>& buffer)
	{
		const size_t& ibSize = buffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size(); i += 6)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}

		cmd->Write(buffer, indices.data(), ibSize, 0, buffer->GetLayout(), BufferReadAccess::Index);
	}

	RenderText2DTask::RenderText2DTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = 1; // Used 1 so that we don't allocate a lot of data here, but rather do it as needed
		indexSpecs.Layout = BufferReadAccess::Index;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "Text2D_VertexBuffer");
		m_IndexBuffer = Buffer::Create(indexSpecs, "Text2D_IndexBuffer");
	}
	
	void RenderText2DTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_Quads.empty())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Text 2D");
		EG_CPU_TIMING_SCOPED("Text 2D");

		Upload(cmd);
		Render(cmd);
	}

	void RenderText2DTask::Upload(const Ref<CommandBuffer>& cmd)
	{
		if (!bUpload)
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Upload Text 2D");
		EG_CPU_TIMING_SCOPED("Upload Text 2D");

		bUpload = false;

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;
		auto& quads = m_Quads;
		using TextVertexType = QuadVertex;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = quads.size() * sizeof(TextVertexType);
		const size_t currentIndexSize = (quads.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(TextVertexType);
			newSize = (newSize + alignment - 1) / alignment * alignment;

			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentIndexSize, ib->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize = (newSize + alignment - 1) / alignment * alignment;

			ib->Resize(newSize);
			UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, vb->GetLayout(), BufferReadAccess::Vertex);
	}

	void RenderText2DTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Text 2D");
		EG_CPU_TIMING_SCOPED("Render Text 2D");

		struct PushData
		{
			glm::vec2 ViewportSize;
			float UIScale;
		} pushData;
		pushData.ViewportSize = glm::vec2(m_Renderer.GetOutput()->GetSize());
		pushData.UIScale = m_Renderer.GetUIScaleSettings_RT().GetScale(pushData.ViewportSize);

		if (m_Pipeline->GetState().ColorAttachments[0].Image != m_Renderer.GetOutput())
			InitPipeline();

		const auto& options = m_Renderer.GetOptions_RT();
		const bool bObjectPickingEnabled = options.bEnableObjectPicking && options.bEnable2DObjectPicking;
		auto& pipeline = (m_Renderer.IsRuntime() && !bObjectPickingEnabled) ? m_PipelineNoEntityID : m_Pipeline;
		pipeline->SetTextureArray(m_Atlases, 0, 0);

		const uint32_t quadsCount = (uint32_t)(m_Quads.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats();
		++stats.DrawCalls;
	}

	void RenderText2DTask::OnResize(glm::uvec2 size)
	{
		m_Pipeline->Resize(size.x, size.y);
		m_PipelineNoEntityID->Resize(size.x, size.y);
	}

	void RenderText2DTask::SetTexts(const std::vector<const Text2DComponent*>& texts)
	{
		std::vector<Text2DComponentData> datas;
		datas.reserve(texts.size());

		for (auto& text : texts)
		{
			Ref<const TextLayout> layout = text->GetLayout();
			if (!layout)
				continue;

			auto& data = datas.emplace_back();
			data.Font = text->GetFontAsset()->GetFont();
			data.Layout = std::move(layout);
			data.Color = text->GetColor();
			data.FontSize = text->GetFontSize();
			data.Anchor = text->GetAnchor();
			data.Pivot = text->GetActualPivot();
			data.Offset = text->GetOffset();
			data.Rotation = text->GetRotation();
			data.EntityID = text->Parent.GetID();
			data.Opacity = text->GetOpacity();
		}

		RenderManager::Submit([components = std::move(datas), task = shared_from_this()](const Ref<CommandBuffer>&)
		{
			auto thisRef = Cast<RenderText2DTask>(task);

			thisRef->bUpload = true;
			thisRef->m_Quads.clear();
			thisRef->m_Atlases.clear();
			thisRef->m_FontAtlases.clear();

			const uint32_t atlasesCount = thisRef->ProcessTexts(components);

			thisRef->m_Atlases.resize(atlasesCount);
			for (auto& atlas : thisRef->m_FontAtlases)
				thisRef->m_Atlases[atlas.second] = atlas.first;
		});
	}

	uint32_t RenderText2DTask::ProcessTexts(const std::vector<Text2DComponentData>& textComponents)
	{
		for (auto& component : textComponents)
		{
			const float fontSize = component.FontSize;
			if (fontSize <= 0.f)
				continue;

			const TextLayout& layout = *component.Layout;
			const uint32_t atlasIndex = GetFontAtlasIndex(m_FontAtlases, component.Font->GetAtlas());

			// Y is flipped since glyphs are laid out with Y pointing up, but NDC has Y pointing down.
			const glm::vec2 toPixels = glm::vec2(fontSize, -fontSize);
			const glm::vec2 pivot = layout.GetBoxPoint(component.Pivot);
			// Rotation is applied in the vertex shader in pixel space, so that it doesn't depend on the viewport's aspect ratio
			const float rotation = glm::radians(component.Rotation);

			for (const auto& glyph : layout.Glyphs)
			{
				for (uint32_t corner = 0; corner < 4; ++corner)
				{
					auto& vertex = m_Quads.emplace_back();
					vertex.Color = component.Color;
					vertex.AtlasIndex = atlasIndex;
					vertex.Position = (glyph.GetPosition(corner) - pivot) * toPixels;
					vertex.TexCoord = glyph.GetAtlasUV(corner);
					vertex.EntityID = component.EntityID;
					vertex.Opacity = component.Opacity;
					vertex.Anchor = component.Anchor;
					vertex.Offset = component.Offset;
					vertex.Rotation = rotation;
				}
			}
		}

		return (uint32_t)m_FontAtlases.size();
	}
	
	void RenderText2DTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetOutput();
		colorAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		colorAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::Zero;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::One;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.FinalLayout = ImageLayoutType::RenderTarget;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		ShaderDefines noObjectIDDefine = { {"EG_NO_OBJECT_ID", ""} };
		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("text/text2D.vert", ShaderType::Vertex, noObjectIDDefine);
		state.FragmentShader = Shader::Create("text/text2D.frag", ShaderType::Fragment, noObjectIDDefine);
		state.ColorAttachments.push_back(colorAttachment);

		if (m_PipelineNoEntityID)
			m_PipelineNoEntityID->SetState(state);
		else
			m_PipelineNoEntityID = PipelineGraphics::Create(state);

		state.VertexShader = Shader::Create("text/text2D.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("text/text2D.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
