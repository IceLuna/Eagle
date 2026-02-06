#include "egpch.h"
#include "TextureCompressor.h"
#include "Eagle/Core/Application.h"

#include <compressonator/compressonator.h>
#include <compressonator/common.h>

namespace Eagle
{
	static CMP_FORMAT(*s_GetCompressionFormatFunc)(uint32_t, TextureCompressor::TextureType) = nullptr;
	static ImageFormat(*s_FromCMPFormatFunc)(CMP_FORMAT) = nullptr;
	static bool(*s_IsFormatSupportedFunc)(ImageFormat) = nullptr;

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

	static CMP_FORMAT GetCompressionFormat_BC(uint32_t numChannels, TextureCompressor::TextureType type)
	{
		// TODO: What about BC7?
		using TT = TextureCompressor::TextureType;

		if (type == TT::HDR)
			return CMP_FORMAT_BC6H;

		if (type == TT::NormalMap)
			return CMP_FORMAT_BC1;

		switch (numChannels)
		{
		case 1:
			return CMP_FORMAT_BC4;
		case 2:
			return CMP_FORMAT_BC5;
		case 3:
			return CMP_FORMAT_BC1;
		case 4:
			return type == TT::RegularWithAlpha ? CMP_FORMAT_BC3 : CMP_FORMAT_BC1;
			// return CMP_FORMAT_BC7;
		default:
			EG_CORE_ASSERT(false);
			return CMP_FORMAT_BC1;
		}
	}

	static CMP_FORMAT GetCompressionFormat_ETC2(uint32_t numChannels, TextureCompressor::TextureType type)
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
			EG_CORE_WARN("Texture compression is not supported by the current device: {}. Currently, only BC and ETC2 compression are supported by the engine", caps.Device);
		}
	}

	void TextureCompressor::Shutdown()
	{
		s_GetCompressionFormatFunc = nullptr;
		s_FromCMPFormatFunc = nullptr;
	}

	bool TextureCompressor::IsCompressionFormatSupported(ImageFormat format)
	{
		return s_IsFormatSupportedFunc ? s_IsFormatSupportedFunc(format) : false;
	}

	TextureCompressor::Result TextureCompressor::Compress(DataBuffer imageData, uint32_t targetNumChannels, uint32_t mipsCount, bool bNormalMap, bool bHDR)
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

		return CompressDecoded(decodedData.GetDataBuffer(), glm::uvec2(width, height), targetNumChannels, mipsCount, bNormalMap, bHDR);
	}
	
	TextureCompressor::Result TextureCompressor::CompressDecoded(DataBuffer imageData, glm::uvec2 size, uint32_t targetNumChannels, uint32_t mipsCount, bool bNormalMap, bool bHDR)
	{
		if (!s_GetCompressionFormatFunc)
			return {}; // Compression is not supported

		const TextureType type = ToTextureType(targetNumChannels, bNormalMap, bHDR);
		const CMP_FORMAT destFormat = s_GetCompressionFormatFunc(targetNumChannels, type);
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
}
