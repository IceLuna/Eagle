#include "egpch.h"
#include "RenderTextUnlitTask.h"

#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/Buffer.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Components/Components.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

#include <codecvt>

namespace Eagle
{
	static std::u32string ToUTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	static bool NextLine(int index, const std::vector<int>& lines)
	{
		for (int line : lines)
		{
			if (line == index)
				return true;
		}
		return false;
	}

	RenderTextUnlitTask::RenderTextUnlitTask(SceneRenderer& renderer, const Ref<Image>& renderTo)
		: RendererTask(renderer)
		, m_ResultImage(renderTo)
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

		m_VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_Text");
		m_IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_Text");

		m_QuadVertices.reserve(s_DefaultVerticesCount);
		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd);
		});
	}

	void RenderTextUnlitTask::SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty)
	{
		if (!bDirty)
			return;

		struct TextData
		{
			glm::mat4 Transform;
			glm::vec3 Color;
			std::u32string Text;
			Ref<Font> Font;
			int EntityID;
			float LineHeightOffset;
			float KerningOffset;
			float MaxWidth;
		};

		std::vector<TextData> datas;
		datas.reserve(texts.size());

		for (auto& text : texts)
		{
			if (text->IsLit())
				continue;

			auto& data = datas.emplace_back();
			data.Transform = Math::ToTransformMatrix(text->GetWorldTransform());
			data.Text = ToUTF32(text->GetText());
			data.Font = text->GetFont();
			data.Color = text->GetColor();
			data.EntityID = text->Parent.GetID();
			data.LineHeightOffset = text->GetLineSpacing();
			data.KerningOffset = text->GetKerning();
			data.MaxWidth = text->GetMaxWidth();
		}

		RenderManager::Submit([this, textComponents = std::move(datas)](Ref<CommandBuffer>&)
		{
			m_UpdateBuffers = true;
			m_QuadVertices.clear();
			m_FontAtlases.clear();
			m_Atlases.clear();

			if (textComponents.empty())
				return;

			uint32_t index = 0;
			for (auto& component : textComponents)
			{
				const auto& fontGeometry = component.Font->GetFontGeometry();
				const auto& metrics = fontGeometry->getMetrics();
				const auto& text = component.Text;
				const auto& atlas = component.Font->GetAtlas();
				uint32_t atlasIndex = index;
				if (m_FontAtlases.size() == EG_MAX_TEXTURES) // Can't be more than EG_MAX_TEXTURES
				{
					EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", EG_MAX_TEXTURES);
					atlasIndex = 0;
				}
				else
				{
					auto it = m_FontAtlases.find(atlas);
					if (it == m_FontAtlases.end())
						m_FontAtlases.emplace(atlas, index++);
					else
						atlasIndex = it->second;
				}

				std::vector<int> nextLines;
				{
					double x = 0.0;
					double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
					double y = -fsScale * metrics.ascenderY;
					int lastSpace = -1;
					for (int i = 0; i < text.size(); i++)
					{
						char32_t character = text[i];
						if (character == '\n')
						{
							x = 0;
							y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
							continue;
						}

						auto glyph = fontGeometry->getGlyph(character);
						if (!glyph)
							glyph = fontGeometry->getGlyph('?');
						if (!glyph)
							continue;

						if (character != ' ')
						{
							// Calc geo
							double pl, pb, pr, pt;
							glyph->getQuadPlaneBounds(pl, pb, pr, pt);
							glm::vec2 quadMin((float)pl, (float)pb);
							glm::vec2 quadMax((float)pr, (float)pt);

							quadMin *= fsScale;
							quadMax *= fsScale;
							quadMin += glm::vec2(x, y);
							quadMax += glm::vec2(x, y);

							if (quadMax.x > component.MaxWidth && lastSpace != -1)
							{
								i = lastSpace;
								nextLines.emplace_back(lastSpace);
								lastSpace = -1;
								x = 0;
								y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
							}
						}
						else
						{
							lastSpace = i;
						}

						double advance = glyph->getAdvance();
						fontGeometry->getAdvance(advance, character, text[i + 1]);
						x += fsScale * advance + component.KerningOffset;
					}
				}

				{
					double x = 0.0;
					double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
					double y = 0.0;
					for (int i = 0; i < text.size(); i++)
					{
						char32_t character = text[i];
						if (character == '\n' || NextLine(i, nextLines))
						{
							x = 0;
							y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
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

						auto& q1 = m_QuadVertices.emplace_back();
						q1.Position = component.Transform * glm::vec4(pl, pb, 0.0f, 1.0f);
						q1.Color = component.Color;
						q1.TexCoord = { l, b };
						q1.EntityID = component.EntityID;
						q1.AtlasIndex = atlasIndex;

						auto& q2 = m_QuadVertices.emplace_back();
						q2.Position = component.Transform * glm::vec4(pl, pt, 0.0f, 1.0f);
						q2.Color = component.Color;
						q2.TexCoord = { l, t };
						q2.EntityID = component.EntityID;
						q2.AtlasIndex = atlasIndex;

						auto& q3 = m_QuadVertices.emplace_back();
						q3.Position = component.Transform * glm::vec4(pr, pt, 0.0f, 1.0f);
						q3.Color = component.Color;
						q3.TexCoord = { r, t };
						q3.EntityID = component.EntityID;
						q3.AtlasIndex = atlasIndex;

						auto& q4 = m_QuadVertices.emplace_back();
						q4.Position = component.Transform * glm::vec4(pr, pb, 0.0f, 1.0f);
						q4.Color = component.Color;
						q4.TexCoord = { r, b };
						q4.EntityID = component.EntityID;
						q4.AtlasIndex = atlasIndex;

						double advance = glyph->getAdvance();
						fontGeometry->getAdvance(advance, character, text[i + 1]);
						x += fsScale * advance + component.KerningOffset;
					}
				}
			}
		
			m_Atlases.resize(EG_MAX_TEXTURES);
			std::fill(m_Atlases.begin(), m_Atlases.end(), Texture2D::BlackTexture);
			for (auto& atlas : m_FontAtlases)
				m_Atlases[atlas.second] = atlas.first;
			m_Pipeline->SetTextureArray(m_Atlases, 0, 0);
		});
	}
	
	void RenderTextUnlitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Text3D Unlit");
		EG_GPU_TIMING_SCOPED(cmd, "Text3D Unlit");

		UpdateBuffers(cmd);
		RenderText(cmd);
	}

	void RenderTextUnlitTask::RenderText(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Text3D Unlit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Unlit");

		const uint32_t quadsCount = (uint32_t)(m_QuadVertices.size() / 4);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&m_Renderer.GetViewProjection()[0][0], nullptr);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderTextUnlitTask::UpdateBuffers(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Text3D Unlit. Upload vertex & index buffers");
		EG_GPU_TIMING_SCOPED(cmd, "Text3D Unlit. Upload vertex & index buffers");

		if (!m_UpdateBuffers)
			return;

		m_UpdateBuffers = false;

		auto& vb = m_VertexBuffer;
		auto& ib = m_IndexBuffer;

		// Reserving enough space to hold Vertex & Index data
		const size_t currentVertexSize = m_QuadVertices.size() * sizeof(QuadVertex);
		const size_t currentIndexSize = (m_QuadVertices.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > vb->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, vb->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(QuadVertex);
			newSize += alignment - (newSize % alignment);

			vb->Resize(newSize);
		}
		if (currentIndexSize > ib->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, ib->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			ib->Resize(newSize);
			UploadIndexBuffer(cmd);
		}

		cmd->Write(vb, m_QuadVertices.data(), currentVertexSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		cmd->TransitionLayout(vb, BufferReadAccess::Vertex, BufferReadAccess::Vertex);
	}

	void RenderTextUnlitTask::UploadIndexBuffer(const Ref<CommandBuffer>& cmd)
	{
		const size_t& ibSize = m_IndexBuffer->GetSize();
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

		cmd->Write(m_IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(m_IndexBuffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}
	
	void RenderTextUnlitTask::InitPipeline()
	{
		ColorAttachment colorAttachment;
		colorAttachment.Image = m_ResultImage;
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

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = m_Renderer.GetGBuffer().Depth;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthCompareOp = CompareOperation::Less;
		depthAttachment.ClearOperation = ClearOperation::Load;

		PipelineGraphicsState state;
		state.VertexShader = ShaderLibrary::GetOrLoad("assets/shaders/text.vert", ShaderType::Vertex);
		state.FragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/text.frag", ShaderType::Fragment);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
