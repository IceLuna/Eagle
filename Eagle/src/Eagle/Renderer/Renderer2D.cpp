#include "egpch.h"
#include "Renderer2D.h"
#include "Renderer.h"

#include "Buffer.h"
#include "Shader.h"
#include "Material.h"
#include "PipelineGraphics.h"
#include "RenderCommandManager.h"
#include "Framebuffer.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/EditorCamera.h"

#include "Eagle/Math/Math.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

#include "../../Eagle-Editor/assets/shaders/common_structures.h"

namespace Eagle
{
	struct RendererMaterial
	{
	public:
		RendererMaterial() = default;
		RendererMaterial(const RendererMaterial&) = default;

		RendererMaterial(const Ref<Material>& material)
		: TintColor(material->TintColor), TilingFactor(material->TilingFactor)
		{}

		RendererMaterial& operator=(const RendererMaterial&) = default;
		RendererMaterial& operator=(const Ref<Material>& material)
		{
			TintColor = material->TintColor;
			TilingFactor = material->TilingFactor;

			uint32_t albedoTextureIndex     = (uint32_t)Renderer::GetTextureIndex(material->GetAlbedoTexture());
			uint32_t metallnessTextureIndex = (uint32_t)Renderer::GetTextureIndex(material->GetMetallnessTexture());
			uint32_t normalTextureIndex     = (uint32_t)Renderer::GetTextureIndex(material->GetNormalTexture());
			uint32_t roughnessTextureIndex  = (uint32_t)Renderer::GetTextureIndex(material->GetRoughnessTexture());
			uint32_t aoTextureIndex         = (uint32_t)Renderer::GetTextureIndex(material->GetAOTexture());

			PackedTextureIndices = PackedTextureIndices2 = 0;
			PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
			PackedTextureIndices |= (metallnessTextureIndex << MetallnessTextureOffset);
			PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

			PackedTextureIndices2 |= (aoTextureIndex << AOTextureOffset);
			PackedTextureIndices2 |= (roughnessTextureIndex & RoughnessTextureMask);

			return *this;
		}

		RendererMaterial& operator=(const Ref<Texture2D>& texture)
		{
			uint32_t albedoTextureIndex = (uint32_t)Renderer::AddTexture(texture);
			PackedTextureIndices |= (albedoTextureIndex & AlbedoTextureMask);

			return *this;
		}
	
	public:
		glm::vec4 TintColor = glm::vec4{ 1.f };
		float TilingFactor = 1.f;

		// [0-9]   bits AlbedoTextureIndex
        // [10-19] bits MetallnessTextureIndex
        // [20-29] bits NormalTextureIndex
		uint32_t PackedTextureIndices  = 0;

		// [0-9]   bits RoughnessTextureIndex
        // [10-19] bits AOTextureIndex
        // [20-31] bits unused
		uint32_t PackedTextureIndices2 = 0;
	};

	struct QuadVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec3 Normal = glm::vec3{ 0.f };
		glm::vec3 WorldTangent = glm::vec3{ 0.f };
		glm::vec3 WorldBitangent = glm::vec3{ 0.f };
		glm::vec3 WorldNormal = glm::vec3{ 0.f };
		glm::vec2 TexCoord = glm::vec2{0.f};
		int EntityID = -1;
		RendererMaterial Material;
	};

	struct LineVertex
	{
		glm::vec4 Color = glm::vec4{0.f, 0.f, 0.f, 1.f};
		glm::vec3 Position = glm::vec3{ 0.f };
	};

	struct Renderer2DData
	{
		std::vector<QuadVertex> QuadVertices;

		GBuffers* GBufferImages = nullptr;

		// Sprites pipeline data
		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;
		Ref<Shader> SpriteVertexShader;
		Ref<Shader> SpriteFragmentShader;
		Ref<PipelineGraphics> SpritePipeline;

		// Lines pipeline data
		Ref<Shader> LineVertexShader;
		Ref<Shader> LineFragmentShader;
		Ref<Image> LineColorImage;
		Ref<Image> LineDepthImage;
		Ref<PipelineGraphics> LinePipeline;

		Ref<PipelineGraphics> CSMPipeline;
		Ref<PipelineGraphics> PointLightSMPipeline;
		std::vector<Ref<Framebuffer>> CSMFramebuffers;
		std::vector<Ref<Framebuffer>> PointLightSMFramebuffers;

		Ref<VulkanSwapchain> Swapchain;

		glm::mat4 ViewProj = glm::mat4(1.f);
		glm::uvec2 ViewportSize = glm::uvec2{ 0 };

		Renderer2D::Statistics Stats[RendererConfig::FramesInFlight];
		uint64_t TexturesUpdatedFrame = 0;

		static constexpr glm::vec4 QuadVertexPosition[4] = { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, -0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f, 0.0f, 1.0f }, { -0.5f, 0.5f, 0.0f, 1.0f } };
		static constexpr glm::vec4 QuadVertexNormal[4]   = { {  0.0f,  0.0f, 1.0f, 0.0f }, { 0.0f,  0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, {  0.0f, 0.0f, 1.0f, 0.0f } };

		static constexpr glm::vec2 TexCoords[4] = { {0.0f, 1.0f}, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f } };
		static constexpr glm::vec3 Edge1 = Renderer2DData::QuadVertexPosition[1] - Renderer2DData::QuadVertexPosition[0];
		static constexpr glm::vec3 Edge2 = Renderer2DData::QuadVertexPosition[2] - Renderer2DData::QuadVertexPosition[0];
		static constexpr glm::vec2 DeltaUV1 = TexCoords[1] - TexCoords[0];
		static constexpr glm::vec2 DeltaUV2 = TexCoords[2] - TexCoords[0];
		static constexpr float f = 1.0f / (DeltaUV1.x * DeltaUV2.y - DeltaUV2.x * DeltaUV1.y);

		static constexpr glm::vec4 Tangent = glm::vec4(f * (DeltaUV2.y * Edge1.x - DeltaUV1.y * Edge2.x),
			f * (DeltaUV2.y * Edge1.y - DeltaUV1.y * Edge2.y),
			f * (DeltaUV2.y * Edge1.z - DeltaUV1.y * Edge2.z),
			0.f);

		static constexpr glm::vec4 Bitangent = glm::vec4(f * (-DeltaUV2.x * Edge1.x + DeltaUV1.x * Edge2.x),
			f * (-DeltaUV2.x * Edge1.y + DeltaUV1.x * Edge2.y),
			f * (-DeltaUV2.x * Edge1.z + DeltaUV1.x * Edge2.z),
			0.f);

		static constexpr size_t DefaultQuadCount     = 512; // How much quads we can render without reallocating
		static constexpr size_t DefaultVerticesCount = DefaultQuadCount * 4;
		static constexpr size_t BaseVertexBufferSize = DefaultVerticesCount * sizeof(QuadVertex); //Allocating enough space to store 2048 vertices
		static constexpr size_t BaseIndexBufferSize  = DefaultQuadCount * (sizeof(Index) * 6);
	};

	static Renderer2DData* s_Data = nullptr;

	static void InitSpritesPipeline()
	{
		s_Data->SpriteVertexShader = ShaderLibrary::GetOrLoad("assets/shaders/sprite.vert", ShaderType::Vertex);
		s_Data->SpriteFragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/sprite.frag", ShaderType::Fragment);

		const auto& size = s_Data->ViewportSize;

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = false;
		colorAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_Data->GBufferImages->Albedo;

		ColorAttachment geometryNormalAttachment;
		geometryNormalAttachment.bClearEnabled = false;
		geometryNormalAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		geometryNormalAttachment.Image = s_Data->GBufferImages->GeometryNormal;

		ColorAttachment shadingNormalAttachment;
		shadingNormalAttachment.bClearEnabled = false;
		shadingNormalAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		shadingNormalAttachment.Image = s_Data->GBufferImages->ShadingNormal;

		ColorAttachment materialAttachment;
		materialAttachment.bClearEnabled = false;
		materialAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		materialAttachment.Image = s_Data->GBufferImages->MaterialData;

		ColorAttachment objectIDAttachment;
		objectIDAttachment.bClearEnabled = false;
		objectIDAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		objectIDAttachment.Image = s_Data->GBufferImages->ObjectID;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_Data->GBufferImages->Depth;
		depthAttachment.bClearEnabled = false;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = s_Data->SpriteVertexShader;
		state.FragmentShader = s_Data->SpriteFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(geometryNormalAttachment);
		state.ColorAttachments.push_back(shadingNormalAttachment);
		state.ColorAttachments.push_back(materialAttachment);
		state.ColorAttachments.push_back(objectIDAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;

		s_Data->SpritePipeline = PipelineGraphics::Create(state);
	}

	static void InitLinesPipeline()
	{
		s_Data->LineVertexShader = ShaderLibrary::GetOrLoad("assets/shaders/line.vert", ShaderType::Vertex);
		s_Data->LineFragmentShader = ShaderLibrary::GetOrLoad("assets/shaders/line.frag", ShaderType::Fragment);

		const auto& size = s_Data->ViewportSize;

		ImageSpecifications colorSpecs;
		colorSpecs.Format = ImageFormat::R8G8B8A8_UNorm;
		colorSpecs.Layout = ImageLayoutType::RenderTarget;
		colorSpecs.Size = { size.x, size.y, 1 };
		colorSpecs.Usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
		s_Data->LineColorImage = Image::Create(colorSpecs, "Line_ColorImage");

		ImageSpecifications depthSpecs;
		depthSpecs.Format = Application::Get().GetRenderContext()->GetDepthFormat();
		depthSpecs.Layout = ImageLayoutType::DepthStencilWrite;
		depthSpecs.Size = { size.x, size.y, 1 };
		depthSpecs.Usage = ImageUsage::DepthStencilAttachment;
		s_Data->LineDepthImage = Image::Create(depthSpecs, "Line_DepthImage");

		ColorAttachment colorAttachment;
		colorAttachment.bClearEnabled = true;
		colorAttachment.InitialLayout = ImageLayoutType::Unknown;
		colorAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		colorAttachment.Image = s_Data->LineColorImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::Unknown;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_Data->LineDepthImage;
		depthAttachment.bClearEnabled = true;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = s_Data->LineVertexShader;
		state.FragmentShader = s_Data->LineFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::Back;
		state.Topology = Topology::Lines;

		s_Data->LinePipeline = PipelineGraphics::Create(state);
	}
	
	static void InitShadowMapPipeline()
	{
		ShaderDefines defines;
		defines["EG_SPRITES"] = "";

		// Directional light
		{
			s_Data->CSMFramebuffers = Renderer::GetCSMFramebuffers();
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = s_Data->CSMFramebuffers[0]->GetImages()[0];

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, defines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;

			s_Data->CSMPipeline = PipelineGraphics::Create(state);
		}
		
		// Point light
		{
			DepthStencilAttachment depthAttachment;
			depthAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
			depthAttachment.Image = Renderer::GetDummyDepthCubeImage();

			ShaderDefines plDefines = defines;
			plDefines["EG_POINT_LIGHT_PASS"] = "";

			PipelineGraphicsState state;
			state.VertexShader = Shader::Create("assets/shaders/shadow_map.vert", ShaderType::Vertex, plDefines);
			state.DepthStencilAttachment = depthAttachment;
			state.CullMode = CullMode::Back;
			state.FrontFace = FrontFaceMode::Clockwise;
			state.bEnableMultiViewRendering = true;
			state.MultiViewPasses = 6;

			s_Data->PointLightSMPipeline = PipelineGraphics::Create(state);
		}
	}

	static void UpdateIndexBuffer(Ref<CommandBuffer>& cmd)
	{
		const size_t ibSize = s_Data->IndexBuffer->GetSize();
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

		cmd->Write(s_Data->IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
	}

	void Renderer2D::Init(GBuffers& gBufferImages)
	{
		s_Data = new Renderer2DData();
		s_Data->Swapchain = Application::Get().GetWindow().GetSwapchain();
		s_Data->ViewportSize = s_Data->Swapchain->GetSize();
		s_Data->GBufferImages = &gBufferImages;

		s_Data->PointLightSMFramebuffers.reserve(256);
		InitSpritesPipeline();
		InitLinesPipeline();
		InitShadowMapPipeline();

		BufferSpecifications vertexSpecs;
		vertexSpecs.Size = Renderer2DData::BaseVertexBufferSize;
		vertexSpecs.Usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;

		BufferSpecifications indexSpecs;
		indexSpecs.Size = Renderer2DData::BaseIndexBufferSize;
		indexSpecs.Usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;

		s_Data->VertexBuffer = Buffer::Create(vertexSpecs, "VertexBuffer_2D");
		s_Data->IndexBuffer = Buffer::Create(indexSpecs, "IndexBuffer_2D");

		s_Data->QuadVertices.reserve(Renderer2DData::DefaultVerticesCount);

		Renderer::Submit(&UpdateIndexBuffer);
	}

	void Renderer2D::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProj)
	{
		s_Data->ViewProj = viewProj;
		s_Data->QuadVertices.clear();
	}

	void Renderer2D::EndScene()
	{
		Renderer::Submit(&Renderer2D::Flush);
	}

	static void ShadowPass(Ref<CommandBuffer>& cmd)
	{
		bool bHasDirLight = false;
		const DirectionalLight& dirLight = Renderer::GetDirectionalLight(&bHasDirLight);

		auto& pointLights = Renderer::GetPointLights();
		auto& vb = s_Data->VertexBuffer;
		auto& ib = s_Data->IndexBuffer;
		const uint32_t quadsCount = (uint32_t)(s_Data->QuadVertices.size() / 4);
		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		
		if (bHasDirLight)
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: CSM Shadow pass");
			EG_CPU_TIMING_SCOPED("Renderer, sprites. CSM Shadow pass");

			struct VertexPushData
			{
				glm::mat4 ViewProj;
			} pushData;


			// For directional light
			for (uint32_t i = 0; i < s_Data->CSMFramebuffers.size(); ++i)
			{
				pushData.ViewProj = dirLight.ViewProj[i];
				cmd->BeginGraphics(s_Data->CSMPipeline, s_Data->CSMFramebuffers[i]);
				cmd->SetGraphicsRootConstants(&pushData, nullptr);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
				s_Data->Stats[frameIndex].DrawCalls++;
			}
		}
	
		if (pointLights.size())
		{
			EG_GPU_TIMING_SCOPED(cmd, "Sprites: Point Lights Shadow pass");
			EG_CPU_TIMING_SCOPED("Renderer, sprites. Point Lights Shadow pass");

			auto& shadowMaps = Renderer::GetPointLightsShadowMaps();
			auto& vpsBuffer = Renderer::GetPointLightsVPBuffer();
			auto& framebuffers = s_Data->PointLightSMFramebuffers;
			auto& pipeline = s_Data->PointLightSMPipeline;
			pipeline->SetBuffer(vpsBuffer, 0, 0);

			uint32_t i = 0;
			for (auto& pointLight : pointLights)
			{
				if (i >= framebuffers.size())
				{
					framebuffers.push_back(Framebuffer::Create({ shadowMaps[i] }, RendererConfig::PointLightSMSize, pipeline->GetRenderPassHandle()));
				}

				cmd->StorageBufferBarrier(vpsBuffer);
				cmd->Write(vpsBuffer, &pointLight.ViewProj[0][0], vpsBuffer->GetSize(), 0, BufferLayoutType::Unknown, BufferLayoutType::StorageBuffer);
				cmd->StorageBufferBarrier(vpsBuffer);

				cmd->BeginGraphics(s_Data->PointLightSMPipeline, framebuffers[i]);
				cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
				cmd->EndGraphics();
				s_Data->Stats[frameIndex].DrawCalls++;
				++i;
			}
		}
	}

	static void RenderQuads(Ref<CommandBuffer>& cmd)
	{
		EG_CPU_TIMING_SCOPED("Renderer. RenderQuads");
		EG_GPU_TIMING_SCOPED(cmd, "Render Quads");

		// Renderer::UsedTextureChanged() is not enough since this might be called later when scene has quads.
		// So if textures were updated somewhat in the past, Renderer::UsedTextureChanged() will return false but we still need to update textures for this pipeline.
		// That's why Renderer::UsedTextureChangedFrame() logic is required
		const uint64_t texturesChangedFrame = Renderer::UsedTextureChangedFrame();
		const bool bUpdateTextures = Renderer::UsedTextureChanged() || (texturesChangedFrame >= s_Data->TexturesUpdatedFrame);
		s_Data->TexturesUpdatedFrame = texturesChangedFrame;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = s_Data->QuadVertices.size() * sizeof(QuadVertex);
		size_t currentIndexSize = (s_Data->QuadVertices.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > s_Data->VertexBuffer->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, s_Data->VertexBuffer->GetSize() * 3 / 2);
			constexpr size_t alignment = 4 * sizeof(QuadVertex);
			newSize += alignment - (newSize % alignment);

			s_Data->VertexBuffer->Resize(newSize);
		}
		if (currentIndexSize > s_Data->IndexBuffer->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, s_Data->IndexBuffer->GetSize() * 3 / 2);
			constexpr size_t alignment = 6 * sizeof(Index);
			newSize += alignment - (newSize % alignment);

			s_Data->IndexBuffer->Resize(newSize);
			UpdateIndexBuffer(cmd);
		}

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		auto& vb = s_Data->VertexBuffer;
		auto& ib = s_Data->IndexBuffer;
		cmd->Write(vb, s_Data->QuadVertices.data(), s_Data->QuadVertices.size() * sizeof(QuadVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);

		if (bUpdateTextures)
			s_Data->SpritePipeline->SetImageSamplerArray(Renderer::GetUsedImages(), Renderer::GetUsedSamplers(), EG_PERSISTENT_SET, 0);
			
		s_Data->SpritePipeline->SetBuffer(Renderer::GetMaterialsBuffer(), EG_PERSISTENT_SET, 1);

		const uint32_t quadsCount = (uint32_t)(s_Data->QuadVertices.size() / 4);

		cmd->BeginGraphics(s_Data->SpritePipeline);
		cmd->SetGraphicsRootConstants(&s_Data->ViewProj[0][0], nullptr);
		cmd->DrawIndexed(vb, ib, quadsCount * 6, 0, 0);
		cmd->EndGraphics();

		s_Data->Stats[frameIndex].DrawCalls++;
		s_Data->Stats[frameIndex].QuadCount += quadsCount / 2; // dividing by 2 because one quad has two faces: front & back
	}

	void Renderer2D::Flush(Ref<CommandBuffer>& cmd)
	{
		if (s_Data->QuadVertices.empty())
			return;

		RenderQuads(cmd);
		ShadowPass(cmd);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<Material>& material, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		DrawQuad(transformMatrix, material, entityID);
	}

	void Renderer2D::DrawQuad(const Transform& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		glm::mat4 transformMatrix = Math::ToTransformMatrix(transform);

		DrawQuad(transformMatrix, subtexture, textureProps, entityID);
	}

	void Renderer2D::DrawQuad(const SpriteComponent& sprite)
	{
		const int entityID = sprite.Parent.GetID();
		const auto& material = sprite.Material;

		if (sprite.bSubTexture)
			Renderer2D::DrawQuad(sprite.GetWorldTransform(), sprite.SubTexture, { material->TintColor, 1.f, material->TilingFactor }, (int)entityID);
		else
			Renderer2D::DrawQuad(sprite.GetWorldTransform(), material, (int)entityID);
	}

	void Renderer2D::DrawDebugLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
	{
	}

	void Renderer2D::OnResized(glm::uvec2 size)
	{
		s_Data->ViewportSize = size;
		s_Data->LineColorImage->Resize({ size, 1 });
		s_Data->LineDepthImage->Resize({ size, 1 });
		s_Data->SpritePipeline->Resize(size.x, size.y);
		s_Data->LinePipeline->Resize(size.x, size.y);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Material>& material, int entityID)
	{
		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const vec3 normal = normalModel * s_Data->QuadVertexNormal[0];
		const vec3 worldNormal = glm::normalize(glm::vec3(transform * s_Data->QuadVertexNormal[0]));
		const vec3 invNormal = -normal;
		const vec3 invWorldNormal = -worldNormal;

		size_t frontFaceVertexIndex = s_Data->QuadVertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();
			vertex.Position = transform * Renderer2DData::QuadVertexPosition[i];
			vertex.Normal = normal;
			vertex.WorldTangent = glm::normalize(glm::vec3(transform * Renderer2DData::Tangent));
			vertex.WorldBitangent = glm::normalize(glm::vec3(transform * Renderer2DData::Bitangent));
			vertex.WorldNormal = worldNormal;
			vertex.TexCoord = Renderer2DData::TexCoords[i];
			vertex.EntityID = entityID;
			vertex.Material = material;
		}

		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();
			vertex = s_Data->QuadVertices[frontFaceVertexIndex++];
			vertex.Normal = invNormal;
			vertex.WorldNormal = invWorldNormal;
		}
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		if (!subtexture)
			return;

		const glm::mat3 normalModel = glm::mat3(glm::transpose(glm::inverse(transform)));
		const vec3 normal = normalModel * s_Data->QuadVertexNormal[0];
		const vec3 worldNormal = glm::normalize(glm::vec3(transform * s_Data->QuadVertexNormal[0]));
		const vec3 invNormal = -normal;
		const vec3 invWorldNormal = -worldNormal;
		
		const uint32_t albedoTextureIndex = (uint32_t)Renderer::GetTextureIndex(subtexture->GetTexture());
		const glm::vec2* spriteTexCoords = subtexture->GetTexCoords();

		size_t frontFaceVertexIndex = s_Data->QuadVertices.size();
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();
		
			vertex.Position = transform * Renderer2DData::QuadVertexPosition[i];
			vertex.Normal = normal;
			vertex.WorldTangent = glm::normalize(glm::vec3(transform * Renderer2DData::Tangent));
			vertex.WorldBitangent = glm::normalize(glm::vec3(transform * Renderer2DData::Bitangent));
			vertex.WorldNormal = worldNormal;
			vertex.TexCoord = spriteTexCoords[i];
			vertex.EntityID = entityID;

			vertex.Material.TintColor = textureProps.TintColor;
			vertex.Material.TilingFactor = textureProps.TilingFactor;
			vertex.Material.PackedTextureIndices = albedoTextureIndex;
		}
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();
			vertex = s_Data->QuadVertices[frontFaceVertexIndex++];
			vertex.Normal = invNormal;
			vertex.WorldNormal = invWorldNormal;
		}
	}

	void Renderer2D::DrawLines()
	{
	}

	void Renderer2D::ResetLinesData()
	{
	}

	void Renderer2D::ResetStats()
	{
		memset(&s_Data->Stats[Renderer::GetCurrentFrameIndex()], 0, sizeof(Renderer2D::Statistics));
	}
	
	Renderer2D::Statistics& Renderer2D::GetStats()
	{
		uint32_t index = Renderer::GetCurrentFrameIndex();
		index = index == 0 ? RendererConfig::FramesInFlight - 2 : index - 1;

		return s_Data->Stats[index]; // Returns stats of the prev frame because current frame stats are not ready yet
	}
}

