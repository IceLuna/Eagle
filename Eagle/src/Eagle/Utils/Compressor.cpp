#include "egpch.h"
#include "Compressor.h"

#include <zstd.h>

namespace Eagle::Compressor
{
	DataBuffer Compress(DataBuffer data)
	{
		const size_t compressedSize = ZSTD_compressBound(data.Size);
		DataBuffer result;
		result.Allocate(compressedSize);

		const size_t actualSize = ZSTD_compress(result.Data, result.Size, data.Data, data.Size, 10);
		if (ZSTD_isError(actualSize))
		{
			// Failed to compress. Return null
			EG_CORE_ERROR("Failed to compress data. {}", ZSTD_getErrorName(actualSize));
			result.Release();
			return {};
		}
		result.Size = actualSize;
		return result;
	}

	size_t CompressFast(DataBuffer data, void* dst, size_t dstCapacity)
	{
		const size_t compressedSize = ZSTD_compressBound(data.Size);
		if (dstCapacity < compressedSize)
		{
			EG_CORE_ERROR("Failed to compress. `Dst` buffer is too small ({} bytes). Required {} bytes", dstCapacity, compressedSize);
			return 0;
		}

		const size_t actualSize = ZSTD_compress(dst, compressedSize, data.Data, data.Size, 1);
		if (ZSTD_isError(actualSize))
		{
			// Failed to compress. Return null
			EG_CORE_ERROR("Failed to compress data. {}", ZSTD_getErrorName(actualSize));
			return 0;
		}
		return actualSize;
	}

	DataBuffer Decompress(DataBuffer data, size_t originalSize)
	{
		size_t size = ZSTD_getFrameContentSize(data.Data, data.Size);
		if (size == ZSTD_CONTENTSIZE_UNKNOWN)
			size = originalSize;

		DataBuffer decompressed;
		decompressed.Allocate(size);

		const size_t error = ZSTD_decompress(decompressed.Data, decompressed.Size, data.Data, data.Size);
		if (ZSTD_isError(error))
		{
			// Failed to decompress. Return null
			EG_CORE_ERROR("Failed to decompress data. {}", ZSTD_getErrorName(error));
			decompressed.Release();
			return {};
		}
		return decompressed;
	}

	size_t Decompress(DataBuffer data, void* dst, size_t dstCapacity)
	{
		size_t size = ZSTD_getFrameContentSize(data.Data, data.Size);
		if (dstCapacity < size)
		{
			EG_CORE_ERROR("Failed to decompress. `Dst` buffer is too small ({} bytes). Required {} bytes", dstCapacity, size);
			return 0;
		}

		const size_t error = ZSTD_decompress(dst, size, data.Data, data.Size);
		if (ZSTD_isError(error))
		{
			// Failed to decompress. Return null
			EG_CORE_ERROR("Failed to decompress data. {}", ZSTD_getErrorName(error));
			return {};
		}
		return size;
	}
	
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer)
	{
		if (originalBuffer.Size != decompressedBuffer.Size)
			return false;

		return memcmp(originalBuffer.Data, decompressedBuffer.Data, originalBuffer.Size) == 0;
	}
}
