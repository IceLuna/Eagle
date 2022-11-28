#include "egpch.h"
#include "Renderer2D.h"
#include "Renderer.h"

#include "Buffer.h"
#include "Shader.h"
#include "Material.h"
#include "PipelineGraphics.h"
#include "RenderCommandManager.h"

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
		: TintColor(material->TintColor), TilingFactor(material->TilingFactor), Shininess(material->Shininess)
		{}

		RendererMaterial& operator=(const RendererMaterial&) = default;
		RendererMaterial& operator=(const Ref<Material>& material)
		{
			TintColor = material->TintColor;
			TilingFactor = material->TilingFactor;
			Shininess = material->Shininess;

			uint32_t diffuseTextureIndex = (uint32_t)Renderer::GetTextureIndex(material->GetDiffuseTexture());
			uint32_t specularTextureIndex = (uint32_t)Renderer::GetTextureIndex(material->GetSpecularTexture());
			uint32_t normalTextureIndex = (uint32_t)Renderer::GetTextureIndex(material->GetNormalTexture());

			PackedTextureIndices = 0;
			PackedTextureIndices |= (normalTextureIndex << NormalTextureOffset);
			PackedTextureIndices |= (specularTextureIndex << SpecularTextureOffset);
			PackedTextureIndices |= diffuseTextureIndex;

			return *this;
		}
	
	public:
		glm::vec4 TintColor = glm::vec4{ 1.f };
		float TilingFactor = 1.f;
		float Shininess = 32.f;
		uint32_t PackedTextureIndices = 0;
	};

	struct QuadVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec3 Normal = glm::vec3{ 0.f };
		glm::vec3 ModelNormal = glm::vec3{ 0.f };
		glm::vec3 Tangent = glm::vec3{ 0.f };
		glm::vec2 TexCoord = glm::vec2{0.f};
		int EntityID = -1;
		RendererMaterial Material;
	};

	struct LineVertex
	{
		glm::vec3 Position = glm::vec3{ 0.f };
		glm::vec4 Color = glm::vec4{0.f, 0.f, 0.f, 1.f};
	};

	struct Renderer2DData
	{
		std::vector<QuadVertex> QuadVertices;

		Ref<Buffer> VertexBuffer;
		Ref<Buffer> IndexBuffer;

		// Sprites pipeline data
		Ref<Shader> SpriteVertexShader;
		Ref<Shader> SpriteFragmentShader;
		Ref<Image> AlbedoImage;
		Ref<Image> NormalImage;
		Ref<Image> DepthImage;
		Ref<PipelineGraphics> SpritePipeline;

		// Lines pipeline data
		Ref<Shader> LineVertexShader;
		Ref<Shader> LineFragmentShader;
		Ref<Image> LineColorImage;
		Ref<Image> LineDepthImage;
		Ref<PipelineGraphics> LinePipeline;

		Ref<VulkanSwapchain> Swapchain;

		glm::mat4 ViewProj = glm::mat4(1.f);
		glm::uvec2 ViewportSize = glm::uvec2{ 0 };

		Renderer2D::Statistics Stats[Renderer::GetConfig().FramesInFlight];
		uint32_t FlushCounter = 0;
		uint32_t IndicesCount = 0;

		static constexpr glm::vec4 QuadVertexPosition[4] = { { -0.5f, -0.5f, 0.f, 1.f }, { 0.5f, -0.5f, 0.f, 1.f }, { 0.5f,  0.5f, 0.f, 1.f }, { -0.5f,  0.5f, 0.f, 1.f } };
		static constexpr glm::vec4 QuadVertexNormal[4] = { { 0.0f,  0.0f, -1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f, 0.0f },  { 0.0f,  0.0f, -1.0f, 0.0f } };

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
		colorAttachment.Image = s_Data->AlbedoImage;
		
		ColorAttachment normalAttachment;
		normalAttachment.bClearEnabled = false;
		normalAttachment.InitialLayout = ImageReadAccess::PixelShaderRead;
		normalAttachment.FinalLayout = ImageReadAccess::PixelShaderRead;
		normalAttachment.Image = s_Data->NormalImage;

		DepthStencilAttachment depthAttachment;
		depthAttachment.InitialLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.FinalLayout = ImageLayoutType::DepthStencilWrite;
		depthAttachment.Image = s_Data->DepthImage;
		depthAttachment.bClearEnabled = false;
		depthAttachment.bWriteDepth = true;
		depthAttachment.DepthClearValue = 1.f;
		depthAttachment.DepthCompareOp = CompareOperation::Less;

		PipelineGraphicsState state;
		state.VertexShader = s_Data->SpriteVertexShader;
		state.FragmentShader = s_Data->SpriteFragmentShader;
		state.ColorAttachments.push_back(colorAttachment);
		state.ColorAttachments.push_back(normalAttachment);
		state.DepthStencilAttachment = depthAttachment;
		state.CullMode = CullMode::None;

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
	
	static void UpdateIndexBuffer(Ref<CommandBuffer>& cmd)
	{
		const size_t ibSize = s_Data->IndexBuffer->GetSize();
		uint32_t offset = 0;
		std::vector<Index> indices(ibSize / sizeof(Index));
		for (size_t i = 0; i < indices.size(); i += 6)
		{
			indices[i + 0] = offset + 2; //0
			indices[i + 1] = offset + 1; //1
			indices[i + 2] = offset + 0; //2

			indices[i + 3] = offset + 0; //2 
			indices[i + 4] = offset + 3; //3
			indices[i + 5] = offset + 2; //0

			offset += 4;
		}

		cmd->Write(s_Data->IndexBuffer, indices.data(), ibSize, 0, BufferLayoutType::Unknown, BufferReadAccess::Index);
	}

	void Renderer2D::Init(const Ref<Image>& albedoImage, const Ref<Image>& normalImage, const Ref<Image>& depthImage)
	{
		s_Data = new Renderer2DData();
		s_Data->Swapchain = Application::Get().GetWindow().GetSwapchain();
		s_Data->ViewportSize = s_Data->Swapchain->GetSize();
		s_Data->AlbedoImage = albedoImage;
		s_Data->NormalImage = normalImage;
		s_Data->DepthImage = depthImage;

		InitSpritesPipeline();
		InitLinesPipeline();

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
		s_Data->IndicesCount = 0;
	}

	void Renderer2D::EndScene()
	{
		Renderer::Submit([](Ref<CommandBuffer>& cmd)
		{
			Flush(cmd);
		});
	}

	void Renderer2D::Flush(Ref<CommandBuffer>& cmd)
	{
		if (s_Data->QuadVertices.empty())
			return;

		// Reserving enough space to hold Vertex & Index data
		size_t currentVertexSize = s_Data->QuadVertices.size() * sizeof(QuadVertex);
		size_t currentIndexSize = (s_Data->QuadVertices.size() / 4) * (sizeof(Index) * 6);

		if (currentVertexSize > s_Data->VertexBuffer->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, s_Data->VertexBuffer->GetSize() * 3 / 2);
			s_Data->VertexBuffer->Resize(newSize);
		}
		if (currentIndexSize > s_Data->IndexBuffer->GetSize())
		{
			size_t newSize = glm::max(currentVertexSize, s_Data->IndexBuffer->GetSize() * 3 / 2);
			s_Data->IndexBuffer->Resize(newSize);
			UpdateIndexBuffer(cmd);
		}

		uint32_t frameIndex = Renderer::GetCurrentFrameIndex();
		auto& vb = s_Data->VertexBuffer;
		auto& ib = s_Data->IndexBuffer;
		cmd->Write(vb, s_Data->QuadVertices.data(), s_Data->QuadVertices.size() * sizeof(QuadVertex), 0, BufferLayoutType::Unknown, BufferReadAccess::Vertex);
		
		if (Renderer::UsedTextureChanged())
			s_Data->SpritePipeline->SetImageSamplerArray(Renderer::GetUsedImages(), Renderer::GetUsedSamplers(), EG_PERSISTENT_SET, 0);
		s_Data->SpritePipeline->SetBuffer(Renderer::GetMaterialsBuffer(), EG_PERSISTENT_SET, 1);

		cmd->BeginGraphics(s_Data->SpritePipeline);
		
		cmd->SetGraphicsRootConstants(&s_Data->ViewProj[0][0], nullptr);
		cmd->DrawIndexed(vb, ib, (uint32_t)((s_Data->QuadVertices.size() / 4) * 6), 0, 0);

		cmd->EndGraphics();

		s_Data->Stats[frameIndex].DrawCalls++;
		s_Data->Stats[frameIndex].QuadCount += (uint32_t)s_Data->QuadVertices.size();
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
		const int entityID = sprite.Parent;
		const auto& material = sprite.Material;

		if (sprite.bSubTexture)
			Renderer2D::DrawQuad(sprite.GetWorldTransform(), sprite.SubTexture, { material->TintColor, 1.f, material->TilingFactor, material->Shininess }, (int)entityID);
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
		glm::vec3 myNormal = glm::mat3(glm::transpose(glm::inverse(transform))) * s_Data->QuadVertexNormal[0];

		constexpr glm::vec2 texCoords[4] = { {0.0f, 0.0f}, { 1.f, 0.f }, { 1.f, -1.f }, { 0.f, -1.f } };
		constexpr glm::vec3 edge1 = Renderer2DData::QuadVertexPosition[1] - Renderer2DData::QuadVertexPosition[0];
		constexpr glm::vec3 edge2 = Renderer2DData::QuadVertexPosition[2] - Renderer2DData::QuadVertexPosition[0];
		constexpr glm::vec2 deltaUV1 = texCoords[1] - texCoords[0];
		constexpr glm::vec2 deltaUV2 = texCoords[2] - texCoords[0];
		constexpr float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		constexpr glm::vec3 tangent = glm::vec3(f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
			f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
			f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z));
		glm::vec3 myTangent = glm::normalize(tangent);
		myTangent = glm::normalize(transform * glm::vec4(myTangent, 0.f));
		glm::vec3 modelNormal = glm::normalize(transform * Renderer2DData::QuadVertexNormal[0]);

		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();
			vertex.Position = transform * Renderer2DData::QuadVertexPosition[i];
			vertex.Normal = myNormal;
			vertex.ModelNormal = modelNormal;
			vertex.Tangent = myTangent;
			vertex.TexCoord = texCoords[i];
			vertex.EntityID = entityID;
			vertex.Material = material;
		}
		s_Data->IndicesCount += 6;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<SubTexture2D>& subtexture, const TextureProps& textureProps, int entityID)
	{
		static const glm::vec2 emptyTextureCoords[4] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
		const glm::vec2* texCoords = emptyTextureCoords;

		if (subtexture)
		{
			texCoords = subtexture->GetTexCoords();
			const Ref<Texture2D>& texture = subtexture->GetTexture();
		}

		glm::vec3 myNormal = glm::mat3(glm::transpose(glm::inverse(transform))) * Renderer2DData::QuadVertexNormal[0];
		glm::vec3 modelNormal = glm::normalize(transform * Renderer2DData::QuadVertexNormal[0]);
		glm::vec4 myTangent = glm::normalize(transform * glm::vec4(1.f, 0.f, 0.f, 0.f));

		const uint32_t diffuseTextureIndex = (uint32_t)Renderer::GetTextureIndex(subtexture->GetTexture());
		for (int i = 0; i < 4; ++i)
		{
			auto& vertex = s_Data->QuadVertices.emplace_back();

			vertex.Position = transform * Renderer2DData::QuadVertexPosition[i];
			vertex.Normal = myNormal;
			vertex.ModelNormal = modelNormal;
			vertex.Tangent = myTangent;
			vertex.TexCoord = texCoords[i];
			vertex.EntityID = entityID;
			vertex.Material.TintColor = textureProps.TintColor;
			vertex.Material.TilingFactor = textureProps.TilingFactor;
			vertex.Material.Shininess = 32.f;
			vertex.Material.PackedTextureIndices = diffuseTextureIndex;
		}

		s_Data->IndicesCount += 6;
	}

	void Renderer2D::DrawCurrentSkybox()
	{
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
		return s_Data->Stats[Renderer::GetCurrentFrameIndex()];
	}
}

