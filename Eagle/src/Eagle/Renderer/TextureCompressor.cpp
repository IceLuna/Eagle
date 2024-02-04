#include "egpch.h"
#include "TextureCompressor.h"
#include "Eagle/Core/Application.h"

#include "Platform/Vulkan/Vulkan.h"
#include "Platform/Vulkan/VulkanUtils.h"

#include <ktx.h>
#include <ktxvulkan.h>
#include <encoder/basisu_comp.h>

namespace Eagle
{
	static std::unordered_map<const void*, basisu::vector<uint8_t>> s_KTX2Data;

	void TextureCompressor::Init()
	{
		basisu::basisu_encoder_init();
	}

	void TextureCompressor::Shutdown()
	{
		basisu::basisu_encoder_deinit();
	}

	void* TextureCompressor::Compress(DataBuffer imageData, glm::uvec2 size, uint32_t mipsCount, bool bNormalMap, bool bNeedAlpha)
	{
		ktxTexture2* texture = nullptr;

		basisu::image img;
		img.resize(size.x, size.y);
		auto& pixels = img.get_pixels();
		memcpy(pixels.data(), imageData.Data, imageData.Size);
		const glm::uvec2 smallestMipSize = size >> (mipsCount - 1);

		basisu::basis_compressor_params basisCompressorParams;

		basisCompressorParams.m_read_source_images = false; // We are not loading the data from a disk
		basisCompressorParams.m_source_images.push_back(img); // We are processing already loaded data
		basisCompressorParams.m_perceptual = false; // No sRGB
		basisCompressorParams.m_mip_srgb = false;
		basisCompressorParams.m_mip_gen = mipsCount > 1;
		basisCompressorParams.m_mip_smallest_dimension = (int)glm::max(smallestMipSize.x, smallestMipSize.y);

		basisCompressorParams.m_uastc = false;
		basisCompressorParams.m_rdo_uastc_multithreading = true;
		basisCompressorParams.m_multithreading = true;
		basisCompressorParams.m_status_output = false;

		basisCompressorParams.m_create_ktx2_file = true;
		basisCompressorParams.m_ktx2_uastc_supercompression = basist::KTX2_SS_ZSTANDARD;

		basisCompressorParams.m_compression_level = 1;
		basisCompressorParams.m_quality_level = basisu::BASISU_QUALITY_MAX;

		basisCompressorParams.m_no_selector_rdo = bNormalMap;
		basisCompressorParams.m_no_endpoint_rdo = bNormalMap;

		basisCompressorParams.m_check_for_alpha = bNeedAlpha;

		basisu::job_pool jpool(std::thread::hardware_concurrency());
		basisCompressorParams.m_pJob_pool = &jpool;

		basisu::basis_compressor basisCompressor;
		if (basisCompressor.init(basisCompressorParams))
		{
			basisu::basis_compressor::error_code basisResult = basisCompressor.process();

			if (basisResult == basisu::basis_compressor::cECSuccess)
			{
				const basisu::vector<uint8_t>& ktx2Data = basisCompressor.get_output_ktx2_file();

				const DataBuffer data{ (void*)ktx2Data.data(), ktx2Data.size() };
				texture = (ktxTexture2*)CreateFromCompressed(data, bNeedAlpha);
			}
			else
			{
				EG_CORE_ERROR("Failed to compress the texture. Basis_compressor has failed!");
			}
		}
		else
		{
			EG_CORE_ERROR("Initialization of basis_compressor has failed!");
		}

		return texture;
	}

	void* TextureCompressor::CreateFromCompressed(DataBuffer compressed, bool bNeedAlpha)
	{
		ktxTexture2* texture = nullptr;

		KTX_error_code result = ktxTexture2_CreateFromMemory((const ktx_uint8_t*)compressed.Data, compressed.Size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
		if (result != KTX_SUCCESS)
			return nullptr;

		// Save ktx data
		{
			basisu::vector<uint8_t> ktx2Data;
			ktx2Data.resize(compressed.Size);
			memcpy(ktx2Data.data(), compressed.Data, compressed.Size);

			s_KTX2Data[texture] = std::move(ktx2Data);
		}

		if (ktxTexture2_NeedsTranscoding(texture))
		{
			ktx_texture_transcode_fmt_e tf = KTX_TTF_NOSELECTION;

			const auto& caps = Application::Get().GetRenderContext()->GetCapabilities();
			khr_df_model_e colorModel = ktxTexture2_GetColorModel_e(texture);

			if (colorModel == KHR_DF_MODEL_UASTC && caps.bTextureCompressionASTC_LDR)
				tf = KTX_TTF_ASTC_4x4_RGBA;
			else if (colorModel == KHR_DF_MODEL_ETC1S && caps.bTextureCompressionETC2)
				tf = KTX_TTF_ETC;
			else if (caps.bTextureCompressionASTC_LDR)
				tf = KTX_TTF_ASTC_4x4_RGBA;
			else if (caps.bTextureCompressionETC2)
			{
				if (bNeedAlpha)
					tf = KTX_TTF_ETC2_RGBA;
				else
					tf = KTX_TTF_ETC1_RGB;
			}
			else if (caps.bTextureCompressionBC)
			{
				if (bNeedAlpha)
					tf = KTX_TTF_BC3_RGBA;
				else
					tf = KTX_TTF_BC1_RGB;
			}

			if (tf == KTX_TTF_NOSELECTION)
				EG_CORE_ERROR("Texture compression is not supported by the GPU");
			else if (result = ktxTexture2_TranscodeBasis(texture, tf, 0); result != KTX_SUCCESS)
				EG_CORE_ERROR("Failed to transcode the texture");
		}

		return texture;
	}

	void TextureCompressor::Destroy(const void* compressedHandle)
	{
		s_KTX2Data.erase(compressedHandle);
		ktxTexture_Destroy((ktxTexture*)compressedHandle);
	}

	DataBuffer TextureCompressor::GetImageData(const void* compressedHandle)
	{
		const ktxTexture2* texture = (const ktxTexture2*)compressedHandle;
		return DataBuffer(texture->pData, texture->dataSize);
	}

	DataBuffer TextureCompressor::GetKTX2Data(const void* compressedHandle)
	{
		auto it = s_KTX2Data.find(compressedHandle);
		if (it != s_KTX2Data.end())
		{
			return DataBuffer(it->second.data(), it->second.size());
		}
		else
		{
			EG_CORE_ERROR("Failed to get KTX2 data");
			return {};
		}
	}
	
	ImageFormat TextureCompressor::GetFormat(const void* compressedHandle)
	{
		const ktxTexture2* texture = (const ktxTexture2*)compressedHandle;
		return VulkanToImageFormat((VkFormat)texture->vkFormat);
	}

	uint32_t TextureCompressor::GetMipsCount(const void* compressedHandle)
	{
		const ktxTexture2* texture = (const ktxTexture2*)compressedHandle;
		return texture->numLevels;
	}

	bool TextureCompressor::IsCompressedSuccessfully(const void* compressedHandle)
	{
		ktxTexture2* texture = (ktxTexture2*)compressedHandle;
		return ktxTexture2_NeedsTranscoding(texture) == false;
	}

	static KTX_error_code IterateLevels(int miplevel, int face, int width, int height, int depth,
		ktx_uint64_t faceLodSize, void* pixels, void* userdata)
	{
		uint64_t* data = (uint64_t*)userdata;
		uint64_t requiredLevel = *data;
		if (requiredLevel != uint64_t(miplevel))
			return KTX_SUCCESS;

		*data = uint64_t(pixels);
		return KTX_SUCCESS;
	}
	
	DataBuffer TextureCompressor::GetMipData(const void* compressedHandle, uint32_t level)
	{
		ktxTexture* texture = (ktxTexture*)compressedHandle;
		const size_t size = ktxTexture_GetImageSize(texture, level);

		// We write the requested level to `userdata`, and the functions writes back to it the memory address of the level
		uint64_t userData = (uint64_t)level;
		ktxTexture_IterateLevels(texture, IterateLevels, &userData);

		return DataBuffer((void*)userData, size);
	}
}
