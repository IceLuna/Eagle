#pragma once

#include "PlatformUtils.h"

namespace Eagle::Compressor
{
	[[nodiscard]] DataBuffer Compress(DataBuffer data);
	size_t CompressFast(DataBuffer src, void* dst, size_t dstCapacity); // Returns compressed size

	[[nodiscard]] DataBuffer Decompress(DataBuffer data, size_t originalSize);
	size_t Decompress(DataBuffer data, void* dst, size_t dstCapacity); // Returns decompressed size

	// This function can be used to validate that compression-decompression succeeded
	// Returns true if data match.
	// @originalBuffer - original buffer that was used for compression
	// @decompressedBuffer - result of decompression
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer);
}
