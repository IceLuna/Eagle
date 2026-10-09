#pragma once

#include "PlatformUtils.h"

namespace Eagle::Compressor
{
	// Called after each compressed chunk with the number of processed input bytes. Return false to abort the compression
	using CompressProgressFunc = std::function<bool(size_t processedBytes, size_t totalBytes)>;

	[[nodiscard]] ScopedDataBuffer Compress(DataBuffer data);
	[[nodiscard]] inline ScopedDataBuffer Compress(const ScopedDataBuffer& data) { return Compress(data.GetDataBuffer()); }

	// Same as `Compress`, but the data is compressed in chunks, and `onProgress` is called after each of them.
	// Returns an empty buffer on failure or if `onProgress` returned false
	[[nodiscard]] ScopedDataBuffer Compress(DataBuffer data, const CompressProgressFunc& onProgress);
	[[nodiscard]] inline ScopedDataBuffer Compress(const ScopedDataBuffer& data, const CompressProgressFunc& onProgress) { return Compress(data.GetDataBuffer(), onProgress); }

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
