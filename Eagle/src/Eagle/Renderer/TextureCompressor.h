#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class TextureCompressor
	{
	public:
		static void Init();
		static void Shutdown();

		// @imageData. Input texture data that needs to be compressed. It can a data that's decompressed from a png file
		// @size. Size of an input texture
		// @bNormalMap. Set to true, if it's a normal map
		// @bNeedAlpha. Set to true, if alpha channel needs to be compressed as well
		// @mipsCount. Should be >= 1. The value of `1` represents the base level. So if it's 1, mips won't be generated
		// @return. Handle to a compressed data. Can be used to retrive data.
		//			Note: it needs to be manually destroyed by calling `Destroy()` function
		static void* Compress(DataBuffer imageData, glm::uvec2 size, uint32_t mipsCount, bool bNormalMap, bool bNeedAlpha);
		static void Destroy(const void* compressedHandle);

		// Already compressed data that's ready to go
		static void* CreateFromCompressed(DataBuffer compressed, bool bNeedAlpha);

		static DataBuffer GetImageData(const void* compressedHandle);
		static DataBuffer GetKTX2Data(const void* compressedHandle);
		static ImageFormat GetFormat(const void* compressedHandle);
		static uint32_t GetMipsCount(const void* compressedHandle);
		static bool IsCompressedSuccessfully(const void* compressedHandle);
		
		// @level. Mip level starting from 0, where `0` is the base level
		static DataBuffer GetMipData(const void* compressedHandle, uint32_t level);
	};
}
