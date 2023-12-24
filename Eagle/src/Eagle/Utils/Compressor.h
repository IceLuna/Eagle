#pragma once

#include "PlatformUtils.h"

namespace Eagle::Compressor
{
	[[nodiscard]] DataBuffer Compress(DataBuffer data);
	[[nodiscard]] DataBuffer Decompress(DataBuffer data, size_t originalSize);

	// This function can be used to validate that compression-decompression succeeded
	// Returns true if data match.
	// @originalBuffer - original buffer that was used for compression
	// @decompressedBuffer - result of decompression
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer);
}
