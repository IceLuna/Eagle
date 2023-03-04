#pragma once

#include "Pipeline.h"
#include "Shader.h"

namespace Eagle
{
	struct Attachment
	{
		Ref<Image> Image;

		ImageLayout InitialLayout;
		ImageLayout FinalLayout;

		bool bClearEnabled = false;
	};

	struct ColorAttachment : Attachment
	{
		glm::vec4 ClearColor{ 0.f };
		BlendState BlendingState;
		bool bBlendEnabled = false;
	};

	struct DepthStencilAttachment : Attachment
	{
		float DepthClearValue = 1.f;
		uint32_t StencilClearValue = 0;
		CompareOperation DepthCompareOp = CompareOperation::Less;
		bool bWriteDepth = true;
	};

	struct PipelineGraphicsState
	{
	public:
		struct VertexInputAttribute
		{
			uint32_t Location = 0;
		};

	public:
		std::vector<ColorAttachment> ColorAttachments;
		std::vector<Attachment> ResolveAttachments;
		ShaderSpecializationInfo VertexSpecializationInfo;
		ShaderSpecializationInfo FragmentSpecializationInfo;
		std::vector<VertexInputAttribute> PerInstanceAttribs;
		Ref<Shader> VertexShader = nullptr;
		Ref<Shader> FragmentShader = nullptr; // Optional
		Ref<Shader> GeometryShader = nullptr; // Optional
		DepthStencilAttachment DepthStencilAttachment;
		Topology Topology = Topology::Triangles;
		CullMode CullMode = CullMode::None;
		FrontFaceMode FrontFace = FrontFaceMode::CounterClockwise;
		glm::uvec2 Size = glm::uvec2(0, 0);
		float LineWidth = 1.0f;
		uint32_t MultiViewPasses = 2; // Used if `bEnableMultiViewRendering` is true
		bool bEnableConservativeRasterization = false;
		bool bImagelessFramebuffer = false;
		bool bEnableMultiViewRendering = false;

		SamplesCount GetSamplesCount() const
		{
			SamplesCount samplesCount = s_InvalidSamplesCount;
			InitSamplesCount(DepthStencilAttachment.Image, samplesCount);
			for (auto& attachment : ColorAttachments)
			{
				InitSamplesCount(attachment.Image, samplesCount);
			}
			EG_CORE_ASSERT(samplesCount != s_InvalidSamplesCount);
			return samplesCount;
		}

	private:
		void InitSamplesCount(const Ref<Image>& image, SamplesCount& samplesCount) const
		{
			if (image)
			{
				if (samplesCount == s_InvalidSamplesCount)
					samplesCount = image->GetSamplesCount();
				EG_CORE_ASSERT(samplesCount == image->GetSamplesCount());
			}
		}

	private:
		static constexpr SamplesCount s_InvalidSamplesCount = static_cast<SamplesCount>(-1);
	};

	class PipelineGraphics : virtual public Pipeline
	{
	protected:
		PipelineGraphics(const PipelineGraphicsState& state) : m_State(state) {}

	public:
		virtual ~PipelineGraphics() = default;

		const PipelineGraphicsState& GetState() const { return m_State; }
		virtual void* GetRenderPassHandle() const = 0;

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// Resizes framebuffer
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		static Ref<PipelineGraphics> Create(const PipelineGraphicsState& state, const Ref<PipelineGraphics>& parentPipeline = nullptr);

	protected:
		PipelineGraphicsState m_State;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};
}
