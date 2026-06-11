#include "egpch.h"
#include "TextureCompressor.h"
#include "Eagle/Core/Application.h"
#include "Eagle/Renderer/RenderManager.h"
#include "Eagle/Renderer/VidWrappers/PipelineCompute.h"
#include "Eagle/Renderer/VidWrappers/Image.h"
#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include <compressonator/compressonator.h>
#include <compressonator/common.h>

namespace Eagle
{
	static CMP_FORMAT(*s_GetCompressionFormatFunc)(uint32_t, TextureCompressor::TextureType, TextureCompressor::Quality) = nullptr;
	static ImageFormat(*s_FromCMPFormatFunc)(CMP_FORMAT) = nullptr;
	static bool(*s_IsFormatSupportedFunc)(ImageFormat) = nullptr;

	static Ref<PipelineCompute> s_BC6HPipeline;
	static Ref<PipelineCompute> s_BC6HCubePipeline;
	static const uint32_t BC_BLOCK_SIZE = 4;

	static uint32_t DivideAndRoundUp(uint32_t x, uint32_t divisor)
	{
		return (x + divisor - 1) / divisor;
	}

	static TextureCompressor::TextureType ToTextureType(uint32_t numChannels, bool bNormalMap, bool bHDR)
	{
		using TT = TextureCompressor::TextureType;

		if (bNormalMap)
			return TT::NormalMap;
		if (bHDR)
			return TT::HDR;

		return numChannels == 4 ? TT::RegularWithAlpha : TT::Regular;
	}

	static CMP_FORMAT ToCMPFormat(TextureCompressor::TextureType type)
	{
		if (type == TextureCompressor::TextureType::HDR)
			return CMP_FORMAT_RGBA_16F;

		return CMP_FORMAT_RGBA_8888;
	}

	static bool CreateCMPTexture(const DataBuffer& imageData, const glm::uvec2& size, TextureCompressor::TextureType type, CMP_MipSet* mipSet)
	{
		const CMP_ChannelFormat format = type == TextureCompressor::TextureType::HDR ? CF_16bit : CF_8bit;
		const CMP_TextureDataType dataType = TDT_ARGB;

		memset(mipSet, 0, sizeof(MipSet));
		CMP_CMIPS CMips;
		if (!CMips.AllocateMipSet(mipSet, format, dataType, TT_2D, size.x, size.y, 1))
		{
			return false; // CMP_ERR_MEM_ALLOC_FOR_MIPSET
		}

		mipSet->m_nMipLevels = 1;
		mipSet->m_format = ToCMPFormat(type);

		// TODO: Test HDR
		if (!CMips.AllocateMipLevelData(CMips.GetMipLevel(mipSet, 0), size.x, size.y, format, dataType))
		{
			return false; // CMP_ERR_MEM_ALLOC_FOR_MIPSET
		}

		CMP_MipLevel* mipLevel = CMips.GetMipLevel(mipSet, 0, 0);
		EG_CORE_ASSERT(mipLevel->m_dwLinearSize == imageData.Size);
		memcpy(mipLevel->m_pbData, imageData.Data, mipLevel->m_dwLinearSize);

		// Assign miplevel 0 to MipSetin pData ref
		// both miplevel pData and mipset pData will point to the same location
		// Typically mipset pData is assign a pointer to the current miplevel data been processed at run time
		mipSet->pData = mipLevel->m_pbData;
		mipSet->dwDataSize = mipLevel->m_dwLinearSize;
		mipSet->dwHeight = mipLevel->m_nHeight;
		mipSet->dwWidth = mipLevel->m_nWidth;

		return true;
	}

	static CMP_FORMAT GetCompressionFormat_BC(uint32_t numChannels, TextureCompressor::TextureType type, TextureCompressor::Quality quality)
	{
		using TT = TextureCompressor::TextureType;

		if (type == TT::HDR)
			return CMP_FORMAT_BC6H;

		if (type == TT::NormalMap)
			return quality == TextureCompressor::Quality::High ? CMP_FORMAT_BC7 : CMP_FORMAT_BC1;

		switch (numChannels)
		{
		case 1:
			return CMP_FORMAT_BC4;
		case 2:
			return CMP_FORMAT_BC5;
		case 3:
			if (quality == TextureCompressor::Quality::High)
				return CMP_FORMAT_BC7;
			return CMP_FORMAT_BC1;
		case 4:
			if (quality == TextureCompressor::Quality::High)
				return CMP_FORMAT_BC7;
			return type == TT::RegularWithAlpha ? CMP_FORMAT_BC3 : CMP_FORMAT_BC1;
		default:
			EG_CORE_ASSERT(false);
			return CMP_FORMAT_BC1;
		}
	}

	static CMP_FORMAT GetCompressionFormat_ETC2(uint32_t numChannels, TextureCompressor::TextureType type, TextureCompressor::Quality quality)
	{
		using TT = TextureCompressor::TextureType;

		if (type == TT::HDR)
			return CMP_FORMAT_Unknown; // Unsupported

		if (type == TT::NormalMap)
			return CMP_FORMAT_ETC2_RGB;

		switch (numChannels)
		{
		case 1:
			return CMP_FORMAT_Unknown; // Unsupported
		case 2:
			return CMP_FORMAT_Unknown; // Unsupported
		case 3:
			return CMP_FORMAT_ETC2_RGB;
		case 4:
			return type == TT::RegularWithAlpha ? CMP_FORMAT_ETC2_RGBA : CMP_FORMAT_ETC2_RGB;
		default:
			EG_CORE_ASSERT(false);
			return CMP_FORMAT_ETC2_RGB;
		}
	}

	static ImageFormat FromCMPFormat_BC(CMP_FORMAT format)
	{
		switch (format)
		{
		case CMP_FORMAT_BC1:
			return ImageFormat::BC1_RGB_UNorm;
		case CMP_FORMAT_BC2:
			return ImageFormat::BC2_UNorm;
		case CMP_FORMAT_BC3:
			return ImageFormat::BC3_UNorm;
		case CMP_FORMAT_BC4:
			return ImageFormat::BC4_UNorm;
		case CMP_FORMAT_BC5:
			return ImageFormat::BC5_UNorm;
		case CMP_FORMAT_BC6H:
			return ImageFormat::BC6H_UFloat16;
		case CMP_FORMAT_BC7:
			return ImageFormat::BC7_UNorm;
		}

		EG_CORE_ERROR("Unknown/Unsupported format: {}", Utils::GetEnumName(format));
		return ImageFormat::Unknown;
	}

	static ImageFormat FromCMPFormat_ETC2(CMP_FORMAT format)
	{
		switch (format)
		{
		case CMP_FORMAT_BC1:
			return ImageFormat::ETC2_RGB_UNorm;
		case CMP_FORMAT_BC3:
			return ImageFormat::ETC2_RGBA_UNorm;
		}

		EG_CORE_ERROR("Unknown/Unsupported format: {}", Utils::GetEnumName(format));
		return ImageFormat::Unknown;
	}

	static bool IsFormatSupported_BC(ImageFormat format)
	{
		switch (format)
		{
		case ImageFormat::BC1_RGB_UNorm:
		case ImageFormat::BC2_UNorm:
		case ImageFormat::BC3_UNorm:
		case ImageFormat::BC4_UNorm:
		case ImageFormat::BC5_UNorm:
		case ImageFormat::BC6H_UFloat16:
		case ImageFormat::BC7_UNorm:
			return true;
		default:
			return false;
		}
	}

	static bool IsFormatSupported_ETC2(ImageFormat format)
	{
		switch (format)
		{
		case ImageFormat::ETC2_RGB_UNorm:
		case ImageFormat::ETC2_RGBA_UNorm:
			return true;
		default:
			return false;
		}
	}

	void TextureCompressor::Init()
	{
		// Do it just once for the lifetime of the executable
		static bool bInitialized = false;
		if (!bInitialized)
		{
			CMP_InitFramework();
			bInitialized = true;
		}

		auto context = Application::Get().GetRenderContext();
		if (!context)
		{
			EG_CORE_ERROR("[TextureCompressor] Failed to get render context");
			return;
		}

		const auto& caps = context->GetCapabilities();
		if (caps.bTextureCompressionBC)
		{
			s_GetCompressionFormatFunc = GetCompressionFormat_BC;
			s_FromCMPFormatFunc = FromCMPFormat_BC;
			s_IsFormatSupportedFunc = IsFormatSupported_BC;
			EG_CORE_INFO("BC compression is enabled");
		}
		else if (caps.bTextureCompressionETC2)
		{
			s_GetCompressionFormatFunc = GetCompressionFormat_ETC2;
			s_FromCMPFormatFunc = FromCMPFormat_ETC2;
			s_IsFormatSupportedFunc = IsFormatSupported_ETC2;
			EG_CORE_INFO("ETC2 compression is enabled. Note: currently, the engine only supports ETC2 compression of RGB8 and RGBA8 textures (LDR)");
		}
		else
		{
			s_GetCompressionFormatFunc = nullptr;
			s_FromCMPFormatFunc = nullptr;
			s_IsFormatSupportedFunc = nullptr;
			EG_CORE_WARN("Texture compression is not supported by the current device: {}. Currently, only BC and ETC2 compressions are supported by the engine", caps.Device);
		}

		PipelineComputeState state{};
		state.ComputeShader = Shader::Create("bc6h.comp", ShaderType::Compute);
		s_BC6HPipeline = PipelineCompute::Create(state);

		state.ComputeShader = Shader::Create("bc6h.comp", ShaderType::Compute, { {"EG_CUBE", ""}});
		s_BC6HCubePipeline = PipelineCompute::Create(state);
	}

	void TextureCompressor::Shutdown()
	{
		s_GetCompressionFormatFunc = nullptr;
		s_FromCMPFormatFunc = nullptr;
		s_BC6HPipeline.reset();
		s_BC6HCubePipeline.reset();
	}

	bool TextureCompressor::IsCompressionFormatSupported(ImageFormat format)
	{
		return s_IsFormatSupportedFunc ? s_IsFormatSupportedFunc(format) : false;
	}

	TextureCompressor::Result TextureCompressor::Compress(DataBuffer imageData, uint32_t targetNumChannels, uint32_t mipsCount, Quality quality, bool bNormalMap, bool bHDR)
	{
		if (!s_GetCompressionFormatFunc)
			return {}; // Compression is not supported

		constexpr int desiredChannels = 4;
		int width = 0, height = 0, channels = 0;

		ScopedDataBuffer decodedData = Utils::LoadTextureFromMemory(imageData, &width, &height, &channels, desiredChannels);
		if (!decodedData)
		{
			EG_CORE_ERROR("Failed to decode the image data (stbi_load_from_memory)");
			return {};
		}

		return CompressDecoded(decodedData.GetDataBuffer(), glm::uvec2(width, height), targetNumChannels, mipsCount, quality, bNormalMap, bHDR);
	}
	
	TextureCompressor::Result TextureCompressor::CompressDecoded(DataBuffer imageData, glm::uvec2 size, uint32_t targetNumChannels, uint32_t mipsCount, Quality compressionQuality, bool bNormalMap, bool bHDR)
	{
		if (!s_GetCompressionFormatFunc || compressionQuality == Quality::Disabled)
			return {}; // Compression is not supported

		const TextureType type = ToTextureType(targetNumChannels, bNormalMap, bHDR);
		const CMP_FORMAT destFormat = s_GetCompressionFormatFunc(targetNumChannels, type, compressionQuality);
		if (destFormat == CMP_FORMAT_Unknown)
		{
			EG_CORE_ERROR("Failed to compress the texture. Desired compression format is not supported!");
			return {};
		}

		CMP_MipSet src;
		if (!CreateCMPTexture(imageData, size, type, &src))
		{
			EG_CORE_ERROR("Failed to compress the texture");
			return {};
		}

		const bool bGenerateMips = mipsCount > 1;
		if (bGenerateMips)
		{
			CMP_INT nMinSize = CMP_CalcMinMipSize(src.m_nHeight, src.m_nWidth, mipsCount);
			if (CMP_GenerateMIPLevels(&src, nMinSize) != CMP_OK)
			{
				EG_CORE_ERROR("Failed to generate mips");
				CMP_FreeMipSet(&src);
				return {};
			}
		}


		const float quality = 0.8f; // TODO: Expose?
		KernelOptions kernelOptions;
		memset(&kernelOptions, 0, sizeof(KernelOptions));
		kernelOptions.format = destFormat;
		kernelOptions.fquality = quality;
		kernelOptions.threads = 0;

		// kernelOptions.bc15 is valid for BC1 to BC5 formats
		{
			// Enable setting channel weights
			kernelOptions.bc15.useChannelWeights = true;
			kernelOptions.bc15.channelWeights[0] = 0.3086f;
			kernelOptions.bc15.channelWeights[1] = 0.6094f;
			kernelOptions.bc15.channelWeights[2] = 0.0820f;
		}

		CMP_MipSet dst;
		memset(&dst, 0, sizeof(CMP_MipSet));

		CMP_ERROR status = CMP_ProcessTexture(&src, &dst, kernelOptions, nullptr);
		if (status != CMP_OK)
		{
			EG_CORE_ERROR("Failed to compress the texture");
			CMP_FreeMipSet(&src);
			CMP_FreeMipSet(&dst);
			return {};
		}

		CMP_FreeMipSet(&src);

		// Just in case if it generates less mips (is it even possible?)
		mipsCount = dst.m_nMipLevels;

		Result result{};
		result.Format = s_FromCMPFormatFunc(dst.m_format);
		result.DataPerMip.reserve(mipsCount);

		for (uint32_t mip = 0; mip < mipsCount; ++mip)
		{
			CMP_MipLevel* mipData = nullptr;
			CMP_GetMipLevel(&mipData, &dst, mip, 0);

			auto& buffer = result.DataPerMip.emplace_back();
			buffer = DataBuffer::Copy(mipData->m_pbData, mipData->m_dwLinearSize);
		}

		CMP_FreeMipSet(&dst);

		return result;
	}
	
	bool TextureCompressor::CompressHDR(const void* imageData, glm::uvec2 size, ImageFormat format, const Ref<Image>& dst)
	{
		const size_t dataSize = CalculateImageMemorySize(format, size.x, size.y);
		glm::uvec2 encodedSize;
		encodedSize.x = DivideAndRoundUp(size.x, BC_BLOCK_SIZE);
		encodedSize.y = DivideAndRoundUp(size.y, BC_BLOCK_SIZE);

		Ref<Image> inputImage;
		Ref<Buffer> encodedBuffer;
		{
			ImageSpecifications specs{};
			specs.Format = format;
			specs.Size = glm::uvec3(size, 1u);
			specs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;

			inputImage = Image::Create(specs);
		}
		{
			BufferSpecifications specs{};
			specs.Size = sizeof(glm::uvec4) * encodedSize.x * encodedSize.y;
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferSrc;
			specs.Layout = BufferLayoutType::StorageBuffer;

			encodedBuffer = Buffer::Create(specs);
		}

		struct PushData
		{
			glm::uvec2 TextureSizeInBlocks;
			glm::vec2 TextureSizeRcp;
		} pushData;
		pushData.TextureSizeInBlocks = encodedSize;
		pushData.TextureSizeRcp = 1.0f / glm::vec2(size);

		s_BC6HPipeline->SetImageSampler(inputImage, Sampler::PointSamplerClamp, 0, 0);
		s_BC6HPipeline->SetBuffer(encodedBuffer, 0, 1);

		std::vector<BufferImageCopy> copyRegion(1);
		copyRegion[0].ImageExtent = glm::uvec3(size, 1u);

		const glm::uvec3 groupSize = s_BC6HPipeline->GetWorkGroupSize();

		Ref<CommandBuffer> cmd = RenderManager::AllocateCommandBuffer(true);
		cmd->Write(inputImage, imageData, dataSize, ImageLayoutType::Unknown, ImageReadAccess::NonPixelShaderRead);
		cmd->Dispatch(s_BC6HPipeline, DivideAndRoundUp(size.x, groupSize.x * BC_BLOCK_SIZE), DivideAndRoundUp(size.y, groupSize.y * BC_BLOCK_SIZE), 1, &pushData);

		cmd->TransitionLayout(encodedBuffer, BufferLayoutType::StorageBuffer, BufferReadAccess::CopySource);
		cmd->TransitionLayout(dst, ImageLayoutType::Unknown, ImageLayoutType::CopyDest);
		cmd->CopyBufferToImage(encodedBuffer, dst, copyRegion);
		cmd->TransitionLayout(dst, ImageLayoutType::CopyDest, ImageReadAccess::PixelShaderRead);

		cmd->End();
		RenderManager::SubmitCommandBuffer(cmd, true);

		return true;
	}
	
	Ref<Image> TextureCompressor::CompressHDR(const Ref<CommandBuffer>& cmd, const Ref<Image>& src)
	{
		Ref<Image> dst;
		{
			ImageSpecifications specs = src->GetSpecs();
			specs.Layout = ImageLayoutType::Unknown;
			specs.Format = ImageFormat::BC6H_UFloat16;
			specs.Usage &= ~ImageUsage::ColorAttachment;
			specs.Usage |= ImageUsage::TransferDst;
			dst = Image::Create(specs, src->GetDebugName());
		}

		const glm::uvec2 size = src->GetSize();
		glm::uvec2 encodedSize;
		encodedSize.x = DivideAndRoundUp(size.x, BC_BLOCK_SIZE);
		encodedSize.y = DivideAndRoundUp(size.y, BC_BLOCK_SIZE);

		Ref<Buffer> encodedBuffer;
		{
			BufferSpecifications specs{};
			specs.Size = sizeof(glm::uvec4) * encodedSize.x * encodedSize.y;
			specs.Usage = BufferUsage::StorageBuffer | BufferUsage::TransferSrc;
			specs.Layout = BufferLayoutType::StorageBuffer;

			encodedBuffer = Buffer::Create(specs);
		}

		struct PushData
		{
			glm::uvec2 TextureSizeInBlocks;
			glm::vec2 TextureSizeRcp;
			uint32_t Face = 0;
		} pushData;
		pushData.TextureSizeInBlocks = encodedSize;

		std::vector<BufferImageCopy> copyRegion(1);

		const bool bCube = src->IsCube();
		const uint32_t faces = bCube ? 6u : 1u;
		const auto& pipeline = bCube ? s_BC6HCubePipeline : s_BC6HPipeline;

		const glm::uvec3 groupSize = pipeline->GetWorkGroupSize();
		const ImageLayout oldSrcLayout = src->GetLayout();

		const uint32_t mipsCount = src->GetMipsCount();
		glm::uvec2 mipSize = size;
		for (uint32_t mip = 0; mip < mipsCount; ++mip)
		{
			const uint32_t numGroupsX = DivideAndRoundUp(mipSize.x, groupSize.x * BC_BLOCK_SIZE);
			const uint32_t numGroupsY = DivideAndRoundUp(mipSize.y, groupSize.y * BC_BLOCK_SIZE);

			pushData.TextureSizeInBlocks = glm::max(encodedSize >> mip, glm::uvec2(1u));
			pushData.TextureSizeRcp = 1.0f / glm::vec2(mipSize);
			copyRegion[0].ImageExtent = glm::uvec3(mipSize, 1u);
			copyRegion[0].ImageMipLevel = mip;

			const ImageView mipView = ImageView{ mip };
			pipeline->SetImageSampler(src, mipView, Sampler::PointSamplerClamp, 0, 0);
			pipeline->SetBuffer(encodedBuffer, 0, 1);

			cmd->TransitionLayout(src, mipView, oldSrcLayout, ImageReadAccess::NonPixelShaderRead);

			for (uint32_t face = 0; face < faces; ++face)
			{
				pushData.Face = face;
				cmd->Dispatch(pipeline, numGroupsX, numGroupsY, 1, &pushData);

				copyRegion[0].ImageArrayLayer = face;
				cmd->TransitionLayout(encodedBuffer, BufferLayoutType::StorageBuffer, BufferReadAccess::CopySource);
				cmd->TransitionLayout(dst, ImageLayoutType::Unknown, ImageLayoutType::CopyDest);
				cmd->CopyBufferToImage(encodedBuffer, dst, copyRegion);
				cmd->TransitionLayout(dst, ImageLayoutType::CopyDest, ImageReadAccess::PixelShaderRead);
				cmd->TransitionLayout(encodedBuffer, BufferReadAccess::CopySource, BufferLayoutType::StorageBuffer);
			}

			pipeline->ResetDescriptors();
			mipSize = glm::max(mipSize >> 1u, glm::uvec2(1u));
		}

		if (oldSrcLayout != ImageLayoutType::Unknown)
		{
			for (uint32_t mip = 0; mip < mipsCount; ++mip)
				cmd->TransitionLayout(src, ImageView{ mip }, ImageReadAccess::NonPixelShaderRead, oldSrcLayout);
		}

		return dst;
	}
}
