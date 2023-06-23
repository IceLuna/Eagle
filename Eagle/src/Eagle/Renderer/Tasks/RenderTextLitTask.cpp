#include "egpch.h"
#include "RenderTextLitTask.h"

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
	static constexpr glm::vec4 s_QuadVertexNormal = { 0.0f,  0.0f, 1.0f, 0.0f };

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

	RenderTextLitTask::RenderTextLitTask(SceneRenderer& renderer)
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

		BufferSpecifications transformsBufferSpecs;
		transformsBufferSpecs.Size = sizeof(glm::mat4) * 100; // 100 transforms
		transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
		transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst | BufferUsage::TransferSrc;

		m_VertexBuffer = Buffer::Create(vertexSpecs, "Text_Lit_VertexBuffer");
		m_IndexBuffer = Buffer::Create(indexSpecs, "Text_Lit_IndexBuffer");
		m_TransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_Lit_TransformsBuffer");

		m_QuadVertices.reserve(s_DefaultVerticesCount);
		RenderManager::Submit([this](Ref<CommandBuffer>& cmd)
		{
			UploadIndexBuffer(cmd);
		});
	}

	void RenderTextLitTask::SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty)
	{
		if (!bDirty)
			return;

		struct TextData
		{
			glm::vec3 Albedo;
			float Roughness;
			glm::vec3 Emissive;
			float Metallness;
			std::u32string Text;
			Ref<Font> Font;
			int EntityID;
			float LineHeightOffset;
			float KerningOffset;
			float MaxWidth;
			float AO;
		};

		std::vector<TextData> datas;
		std::vector<glm::mat4> tempTransforms;
		datas.reserve(texts.size());
		tempTransforms.reserve(texts.size());

		for (auto& text : texts)
		{
			if (!text->IsLit())
				continue;
			auto& data = datas.emplace_back();
			data.Text = ToUTF32(text->GetText());
			data.Font = text->GetFont();
			data.Albedo = text->GetAlbedoColor();
			data.Emissive = text->GetEmissiveColor();
			data.Roughness = glm::max(EG_MIN_ROUGHNESS, text->GetRoughness());
			data.Metallness = text->GetMetallness();
			data.AO = text->GetAO();
			data.EntityID = text->Parent.GetID();
			data.LineHeightOffset = text->GetLineSpacing();
			data.KerningOffset = text->GetKerning();
			data.MaxWidth = text->GetMaxWidth();

			tempTransforms.emplace_back(Math::ToTransformMatrix(text->GetWorldTransform()));
		}

		RenderManager::Submit([this, textComponents = std::move(datas), transforms = std::move(tempTransforms)](Ref<CommandBuffer>&) mutable
		{
			m_UploadQuads = true;
			m_UploadTransforms = true;
			m_QuadVertices.clear();
			m_FontAtlases.clear();
			m_Atlases.clear();
			m_Transforms = std::move(transforms);

			if (textComponents.empty())
				return;

			uint32_t atlasCurrentIndex = 0;
			const size_t componentsCount = textComponents.size();
			for (size_t i = 0; i < componentsCount; ++i)
			{
				auto& component = textComponents[i];
				const auto& fontGeometry = component.Font->GetFontGeometry();
				const auto& metrics = fontGeometry->getMetrics();
				const auto& text = component.Text;
				const auto& atlas = component.Font->GetAtlas();
				uint32_t atlasIndex = atlasCurrentIndex;
				if (m_FontAtlases.size() == EG_MAX_TEXTURES) // Can't be more than EG_MAX_TEXTURES
				{
					EG_CORE_CRITICAL("Not enough samplers to store all font atlases! Max supported fonts: {}", EG_MAX_TEXTURES);
					atlasIndex = 0;
				}
				else
				{
					auto it = m_FontAtlases.find(atlas);
					if (it == m_FontAtlases.end())
						m_FontAtlases.emplace(atlas, atlasCurrentIndex++);
					else
						atlasIndex = it->second;
				}
				
				const double spaceAdvance = fontGeometry->getGlyph(' ')->getAdvance();
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

						const bool bIsTab = character == '\t';
						if (character == ' ' || bIsTab)
						{
							character = ' '; // treat tabs as spaces
							double advance = spaceAdvance;
							if (i < text.size() - 1)
							{
								char nextCharacter = text[i + 1];
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
					const uint32_t transformIndex = uint32_t(i);

					for (int i = 0; i < text.size(); i++)
					{
						char32_t character = text[i];
						if (character == '\n' || NextLine(i, nextLines))
						{
							x = 0;
							y -= fsScale * metrics.lineHeight + component.LineHeightOffset;
							continue;
						}

						const bool bIsTab = character == '\t';
						if (character == ' ' || bIsTab)
						{
							character = ' '; // treat tabs as spaces
							double advance = spaceAdvance;
							if (i < text.size() - 1)
							{
								char nextCharacter = text[i + 1];
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

						auto& q1 = m_QuadVertices.emplace_back();
						q1.Position = glm::vec4(pl, pb, 0.0f, 1.0f);
						q1.AlbedoRoughness = glm::vec4(component.Albedo, component.Roughness);
						q1.EmissiveMetallness = glm::vec4(component.Emissive, component.Metallness);
						q1.AO = component.AO;
						q1.TexCoord = { l, b };
						q1.EntityID = component.EntityID;
						q1.AtlasIndex = atlasIndex;
						q1.TransformIndex = transformIndex;

						auto& q2 = m_QuadVertices.emplace_back();
						q2 = q1;
						q2.Position = glm::vec4(pl, pt, 0.0f, 1.0f);
						q2.TexCoord = { l, t };

						auto& q3 = m_QuadVertices.emplace_back();
						q3 = q2;
						q3.Position = glm::vec4(pr, pt, 0.0f, 1.0f);
						q3.TexCoord = { r, t };

						auto& q4 = m_QuadVertices.emplace_back();
						q4 = q3;
						q4.Position = glm::vec4(pr, pb, 0.0f, 1.0f);
						q4.TexCoord = { r, b };

						// back face, they have NOT inverted normals
						m_QuadVertices.emplace_back(q1);
						m_QuadVertices.emplace_back(q2);
						m_QuadVertices.emplace_back(q3);
						m_QuadVertices.emplace_back(q4);

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
		});
	}

	void RenderTextLitTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (m_QuadVertices.empty())
			return;

		EG_CPU_TIMING_SCOPED("Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Text3D Lit");

		UpdateBuffers(cmd);
		UploadTransforms(cmd);
		RenderText(cmd);
	}

	void RenderTextLitTask::RenderText(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Render Text3D Lit");
		EG_GPU_TIMING_SCOPED(cmd, "Render Text3D Lit");

		struct PushData
		{
			glm::mat4 ViewProj;
			glm::mat4 PrevViewProj;
		} pushData;
		pushData.ViewProj = m_Renderer.GetViewProjection();

		m_Pipeline->SetBuffer(m_TransformsBuffer, 0, 0);
		if (bMotionRequired)
		{
			pushData.PrevViewProj = m_Renderer.GetPrevViewProjection();
			m_Pipeline->SetBuffer(m_PrevTransformsBuffer, 0, 1);
		}
		m_Pipeline->SetTextureArray(m_Atlases, 1, 0);

		const uint32_t quadsCount = (uint32_t)(m_QuadVertices.size() / 4);
		cmd->BeginGraphics(m_Pipeline);
		cmd->SetGraphicsRootConstants(&pushData, nullptr);
		cmd->DrawIndexed(m_VertexBuffer, m_IndexBuffer, quadsCount * 6, 0, 0);
		cmd->EndGraphics();
	}

	void RenderTextLitTask::UpdateBuffers(const Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Text3D Lit. Upload vertex & index buffers");
		EG_GPU_TIMING_SCOPED(cmd, "Text3D Lit. Upload vertex & index buffers");

		if (!m_UploadQuads)
			return;

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
		m_UploadQuads = false;
	}

	void RenderTextLitTask::UploadIndexBuffer(const Ref<CommandBuffer>& cmd)
	{
		const size_t& ibSize = m_IndexBuffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size();)
		{
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
			i += 6;

			indices[i + 0] = offset + 2;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 0;

			indices[i + 3] = offset + 0;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 2;

			offset += 4;
			i += 6;
		}

		cmd->Write(m_IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
		cmd->TransitionLayout(m_IndexBuffer, BufferReadAccess::Index, BufferReadAccess::Index);
	}
	
	void RenderTextLitTask::UploadTransforms(const Ref<CommandBuffer>& cmd)
	{
		EG_GPU_TIMING_SCOPED(cmd, "Text3D Lit. Upload Transforms buffer");
		EG_CPU_TIMING_SCOPED("Text3D Lit. Upload Transforms buffer");

		if (!m_UploadTransforms)
			return;

		m_UploadTransforms = false;

		const auto& transforms = m_Transforms;
		auto& gpuBuffer = m_TransformsBuffer;

		const size_t currentBufferSize = transforms.size() * sizeof(glm::mat4);
		if (currentBufferSize > gpuBuffer->GetSize())
		{
			gpuBuffer->Resize((currentBufferSize * 3) / 2);

			if (m_PrevTransformsBuffer)
				m_PrevTransformsBuffer.reset();
		}

		if (bMotionRequired && m_PrevTransformsBuffer)
			cmd->CopyBuffer(m_TransformsBuffer, m_PrevTransformsBuffer, 0, 0, m_TransformsBuffer->GetSize());

		cmd->Write(gpuBuffer, transforms.data(), currentBufferSize, 0, BufferLayoutType::StorageBuffer, BufferLayoutType::StorageBuffer);
		cmd->StorageBufferBarrier(gpuBuffer);

		if (bMotionRequired && !m_PrevTransformsBuffer)
		{
			BufferSpecifications transformsBufferSpecs;
			transformsBufferSpecs.Size = m_TransformsBuffer->GetSize();
			transformsBufferSpecs.Layout = BufferLayoutType::StorageBuffer;
			transformsBufferSpecs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
			m_PrevTransformsBuffer = Buffer::Create(transformsBufferSpecs, "Text_Lit_PrevTransformsBuffer");

			cmd->CopyBuffer(m_TransformsBuffer, m_PrevTransformsBuffer, 0, 0, m_TransformsBuffer->GetSize());
		}
	}

	void RenderTextLitTask::InitPipeline()
	{
		const auto& gbuffer = m_Renderer.GetGBuffer();

		ColorAttachment colorAttachment;
		colorAttachment.ClearOperation = ClearOperation::Load;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = gbuffer.AlbedoRoughness;

		ColorAttachment geometry_shading_NormalsAttachment;
		geometry_shading_NormalsAttachment.ClearOperation = ClearOperation::Load;
		geometry_shading_NormalsAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometry_shading_NormalsAttachment.Image = gbuffer.Geometry_Shading_Normals;

		ColorAttachment emissiveAttachment;
		emissiveAttachment.ClearOperation = ClearOperation::Load;
		emissiveAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		emissiveAttachment.Image = gbuffer.Emissive;

		ColorAttachment materialAttachment;
		materialAttachment.ClearOperation = ClearOperation::Load;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.Image = gbuffer.MaterialData;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.ClearOperation = ClearOperation::Load;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.Image = gbuffer.ObjectID;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = gbuffer.Depth;
		depthAttachment.ClearOperation = ClearOperation::Load;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		ShaderDefines defines;
		if (bMotionRequired)
			defines["EG_MOTION"] = "";

		PipelineGraphicsState state;
		state.VertexShader = Shader::Create("assets/shaders/text_lit.vert", ShaderType::Vertex, defines);
		state.FragmentShader = Shader::Create("assets/shaders/text_lit.frag", ShaderType::Fragment, defines);
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometry_shading_NormalsAttachment);
		state.ColorAttachments.push_back(emissiveAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		if (bMotionRequired)
		{
			ColorAttachment velocityAttachment;
			velocityAttachment.Image = gbuffer.Motion;
			velocityAttachment.InitialLayout = ImageLayoutType::Unknown;
			velocityAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			velocityAttachment.ClearOperation = ClearOperation::Clear;
			state.ColorAttachments.push_back(velocityAttachment);
		}
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		m_Pipeline = PipelineGraphics::Create(state);
	}
}
