#include "egpch.h"
#include "Compressor.h"

#include <zlib.h>

namespace Eagle::Compressor
{
	DataBuffer Compress(DataBuffer data)
	{
		// The array that is to hold the compressed data must start out being AT LEAST 0.1% larger than the original size of the data, + 12 extra bytes.
		// So, we'll just play it safe and alloated 1.1x as much memory + 12 bytes (110% original + 12 bytes).
		// When zlib performs compression, it will need a bit of extra room to do its work.
		// When the compress() routine returns, the compressedData array will have
		// been AUTOMATICALLY RESIZED by ZLIB to being a smaller, compressed size.

		// zlib::compress takes a `uLong` as a size param. So make sure we don't overflow
		EG_CORE_ASSERT(data.Size <= std::numeric_limits<uLong>::max());

		float memoryIncreaseCoef = 1.1f;
		int zResult = Z_OK;
		do
		{
			uLongf sizeDataCompressed = size_t(data.Size * memoryIncreaseCoef) + 12ull;
			ScopedDataBuffer temp;
			temp.Allocate(sizeDataCompressed);

			zResult = compress2((Bytef*)temp.Data(), &sizeDataCompressed, (Bytef*)data.Data, (uLong)data.Size, Z_DEFAULT_COMPRESSION);
			if (zResult == Z_OK)
			{
				DataBuffer result;
				result.Allocate(sizeDataCompressed);
				result.Write(temp.Data(), sizeDataCompressed);
				return result;
			}
			memoryIncreaseCoef += 0.1f; // Increase next mem allocation in case we failed
		} while (zResult == Z_BUF_ERROR); // Z_BUF_ERROR means that the buffer wasn't big enough, so we try again

		return {};
	}

	DataBuffer Decompress(DataBuffer data, size_t originalSize)
	{
		// You never know how big compressed data will blow out to be.
		// It can blow up to being anywhere from 2 times as big, or it can be (exactly the same size),
		// or it can be up to 10 times as big or even bigger! So, you can tell its a really bad idea
		// to try to GUESS the proper size that the uncompressed data will end up being.
		// You're SUPPOSED TO HAVE SAVED THE INFORMATION about the original size of the data at the time you compress it.

		// zlib::uncompress takes a `uLong` as a size param. So make sure we don't overflow
		EG_CORE_ASSERT(data.Size <= std::numeric_limits<uLong>::max());

		float memoryIncreaseCoef = 1.0f;
		int zResult = Z_OK;
		do
		{
			uLongf sizeDataCompressed = size_t(originalSize * memoryIncreaseCoef);
			ScopedDataBuffer temp;
			temp.Allocate(sizeDataCompressed);

			zResult = uncompress((Bytef*)temp.Data(), &sizeDataCompressed, (Bytef*)data.Data, (uLong)data.Size);
			if (zResult == Z_OK)
			{
				DataBuffer result;
				result.Allocate(sizeDataCompressed);
				result.Write(temp.Data(), sizeDataCompressed);
				return result;
			}
			memoryIncreaseCoef += 0.05f; // Increase next mem allocation in case we failed
		} while (zResult == Z_BUF_ERROR); // Z_BUF_ERROR means that the buffer wasn't big enough, so we try again

		return {};
	}
	
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer)
	{
		if (originalBuffer.Size != decompressedBuffer.Size)
			return false;

		return memcmp(originalBuffer.Data, decompressedBuffer.Data, originalBuffer.Size) == 0;
	}
}
