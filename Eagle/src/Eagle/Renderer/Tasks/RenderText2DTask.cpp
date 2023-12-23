#include "egpch.h"
#include "RenderText2DTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"
#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include <codecvt>

namespace Eagle
{
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

		cmd->Write(buffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(buffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}

	RenderText2DTask::RenderText2DTask(SceneRenderer& renderer)
		: RendererTask(renderer)
	{
		InitPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = s_BaseVertexBufferSize;
		vertexSpecs.Layout = BufferReadAccess::Vertex;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = s_BaseIndexBufferSize;
		indexSpecs.Layout = BufferReadAccess::Index;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "Text2D_VertexBuffer");
		m_IndexBuffer = Buffer::Create(indexSpecs, "Text2D_IndexBuffer");

		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd, m_IndexBuffer);
		});
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
			newSize += alignment - (newSize % alignment);

			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, ib->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			ib->Resize(newSize);
			UploadIndexBuffer(cmd, ib);
		}

		cmd->Write(vb, quads.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderText2DTask::Render(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Render Text 2D");
		EG_CPU_TIMING_SCOPED("Render Text 2D");

		const auto& options = m_Renderer.GetOptions_RT();
		const bool bObjectPickingEnabled = options.bEnableObjectPicking && options.bEnable2DObjectPicking;
		auto& pipeline = (m_Renderer.IsRuntime() && !bObjectPickingEnabled) ? m_PipelineNoEntityID : m_Pipeline;
		pipeline->SetTextureArray(m_Atlases, 0, 0);

		const uint32_t quadsCount = (uint32_t)(m_Quads.size() / 4);
		cmd->BeginGraphics(pipeline);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();

		auto& stats = m_Renderer.GetStats2D();
		++stats.DrawCalls;
		stats.QuadCount += quadsCount;
	}

	void RenderText2DTask::OnResize(glm::uvec2 size)
	{
		m_Pipeline->Resize(size.x, size.y);
		m_PipelineNoEntityID->Resize(size.x, size.y);
	}

	static std::u32string ToUTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	void RenderText2DTask::SetTexts(const std::vector<const Text2DComponent*>& texts, bool bDirty)
	{
		if (!bDirty)
			return;

		std::vector<Text2DComponentData> datas;
		datas.reserve(texts.size());

		for (auto& text : texts)
		{
			const auto& font = text->GetFont();
			if (!font)
				continue;

			auto& data = datas.emplace_back();
			data.Font = font;
			data.Text = ToUTF32(text->GetText());
			data.Color = text->GetColor();
			data.LineSpacing = text->GetLineSpacing();
			data.Pos = text->GetPosition();
			data.Scale = text->GetScale();
			data.Rotation = text->GetRotation();
			data.KerningOffset = text->GetKerning();
			data.MaxWidth = text->GetMaxWidth();
			data.EntityID = text->Parent.GetID();
			data.Opacity = text->GetOpacity();
		}

		RenderManager::Submit([components = std::move(datas), this](const Ref<CommandBuffer>&)
		{
			bUpload = true;
			m_Quads.clear();
			m_Atlases.clear();
			m_FontAtlases.clear();

			const uint32_t atlasesCount = ProcessTexts(components);

			m_Atlases.resize(atlasesCount);
			for (auto& atlas : m_FontAtlases)
				m_Atlases[atlas.second] = atlas.first;
		});
	}

	uint32_t RenderText2DTask::ProcessTexts(const std::vector<Text2DComponentData>& textComponents)
	{
		if (textComponents.empty())
			return 0;

		uint32_t atlasCurrentIndex = 0u;
		for (auto& component : textComponents)
		{
			const auto& fontGeometry = component.Font->GetFontGeometry();
			const auto& metrics = fontGeometry->getMetrics();
			const auto& text = component.Text;
			const auto& atlas = component.Font->GetAtlas();
			uint32_t atlasIndex = atlasCurrentIndex;

			auto it = m_FontAtlases.find(atlas);
			if (it == m_FontAtlases.end())
			{
				if (m_FontAtlases.size() == RendererConfig::MaxTextures) // Can't be more than EG_MAX_TEXTURES
				{
					EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", RendererConfig::MaxTextures);
					atlasIndex = 0;
				}
				else
					m_FontAtlases.emplace(atlas, atlasCurrentIndex++);
			}
			else
				atlasIndex = it->second;

			const double spaceAdvance = fontGeometry->getGlyph(' ')->getAdvance();
			std::vector<int> nextLines = Font::GetNextLines(metrics, fontGeometry, text, spaceAdvance,
				component.LineSpacing, component.KerningOffset, component.MaxWidth);

			{
				const glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(component.Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
				const glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(component.Scale.x, -component.Scale.y, 1.f));
				const glm::mat4 transform = scaleMat * rotate;

				double x = 0.0;
				double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
				double y = 0.0;
				const size_t textSize = text.size();
				for (int i = 0; i < textSize; i++)
				{
					char32_t character = text[i];
					if (character == '\n' || Font::NextLine(i, nextLines))
					{
						x = 0;
						y -= fsScale * metrics.lineHeight + component.LineSpacing;
						continue;
					}

					const bool bIsTab = character == '\t';
					if (character == ' ' || bIsTab)
					{
						character = ' '; // treat tabs as spaces
						double advance = spaceAdvance;
						if (i < textSize - 1)
						{
							char32_t nextCharacter = text[i + 1];
							if (nextCharacter == '\t')
								nextCharacter = ' ';
							fontGeometry->getAdvance(advance, character, nextCharacter);
						}

						// Tab is 4 spaces
						x += (fsScale * advance + component.KerningOffset) * (bIsTab ? 4.0 : 1.0);
						continue;
					}

					auto glyph = fontGeometry->getGlyph(character);
					if (!glyph)
						glyph = fontGeometry->getGlyph('?');
					if (!glyph)
						continue;

					double l, b, r, t;
					glyph->getQuadAtlasBounds(l, b, r, t);

					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);

					pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
					pl += x, pb += y, pr += x, pt += y;

					double texelWidth = 1. / atlas->GetWidth();
					double texelHeight = 1. / atlas->GetHeight();
					l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

					const size_t q1Index = m_Quads.size();
					{
						auto& q1 = m_Quads.emplace_back();
						q1.Color = component.Color;
						q1.AtlasIndex = atlasIndex;
						q1.Position = glm::vec2(transform * glm::vec4(pl, pb, 0.f, 1.f)) + component.Pos;
						q1.TexCoord = { l, b };
						q1.EntityID = component.EntityID;
						q1.Opacity = component.Opacity;
					}

					{
						auto& q2 = m_Quads.emplace_back();
						q2 = m_Quads[q1Index];
						q2.Position = glm::vec2(transform * glm::vec4(pl, pt, 0.f, 1.f)) + component.Pos;
						q2.TexCoord = { l, t };
					}

					{
						auto& q3 = m_Quads.emplace_back();
						q3 = m_Quads[q1Index];
						q3.Position = glm::vec2(transform * glm::vec4(pr, pt, 0.f, 1.f)) + component.Pos;
						q3.TexCoord = { r, t };
					}

					{
						auto& q4 = m_Quads.emplace_back();
						q4 = m_Quads[q1Index];
						q4.Position = glm::vec2(transform * glm::vec4(pr, pb, 0.f, 1.f)) + component.Pos;
						q4.TexCoord = { r, b };
					}

					if (i + 1 < textSize)
					{
						double advance = glyph->getAdvance();
						fontGeometry->getAdvance(advance, character, text[i + 1]);
						x += fsScale * advance + component.KerningOffset;
					}
				}
			}
		}

		return atlasCurrentIndex;
	}
	
	void RenderText2DTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_Renderer.GetHDROutput();
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.ClearOperation = ClearOperation::Load;

		colorAttachment.bBlendEnabled = true;
		colorAttachment.BlendingState.BlendOp = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrc = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDst = BlendFactor::OneMinusSrcAlpha;

		colorAttachment.BlendingState.BlendOpAlpha = BlendOperation::Add;
		colorAttachment.BlendingState.BlendSrcAlpha = BlendFactor::SrcAlpha;
		colorAttachment.BlendingState.BlendDstAlpha = BlendFactor::OneMinusSrcAlpha;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.Image = m_Renderer.GetGBuffer().ObjectID;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.ClearOperation = ClearOperation::Load;

		ShaderDefines noObjectIDDefine = { {"EG_NO_OBJECT_ID", ""} };
		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/text2D.vert", ShaderType::Vertex, noObjectIDDefine);
		state.FragmentShader = Shader::Create("assets/shaders/text2D.frag", ShaderType::Fragment, noObjectIDDefine);
		state.ColorAttachments.push_back(colorAttachment);
		state.CullMode = CullMode::Front;

		if (m_PipelineNoEntityID)
			m_PipelineNoEntityID->SetState(state);
		else
			m_PipelineNoEntityID = PipelineGraphics::Create(state);

		state.VertexShader = Shader::Create("assets/shaders/text2D.vert", ShaderType::Vertex);
		state.FragmentShader = Shader::Create("assets/shaders/text2D.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (m_Pipeline)
			m_Pipeline->SetState(state);
		else
			m_Pipeline = PipelineGraphics::Create(state);
	}
}
