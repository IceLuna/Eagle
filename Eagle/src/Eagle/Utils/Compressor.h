#pragma once

#include "PlatformUtils.h"

namespace Eagle::Compressor
{
	[[nodiscard]] ScopedDataBuffer Compress(DataBuffer data);
	[[nodiscard]] inline ScopedDataBuffer Compress(const ScopedDataBuffer& data) { return Compress(data.GetDataBuffer()); }

	size_t CompressFast(DataBuffer src, void* dst, size_t dstCapacity); // Returns compressed size. 0 on failure
	inline size_t CompressFast(const ScopedDataBuffer& src, void* dst, size_t dstCapacity) { return CompressFast(src.GetDataBuffer(), dst, dstCapacity); }

	// Returns an empty buffer on failure
	[[nodiscard]] ScopedDataBuffer Decompress(DataBuffer data);
	[[nodiscard]] inline ScopedDataBuffer Decompress(const ScopedDataBuffer& data) { return Decompress(data.GetDataBuffer()); }

	size_t Decompress(DataBuffer data, void* dst, size_t dstCapacity); // Returns decompressed size. 0 on failure
	inline size_t Decompress(const ScopedDataBuffer& data, void* dst, size_t dstCapacity) { return Decompress(data.GetDataBuffer(), dst, dstCapacity); }

	// This function can be used to validate that compression-decompression succeeded
	// Returns true if data match.
	// @originalBuffer - original buffer that was used for compression
	// @decompressedBuffer - result of decompression
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer);
}
