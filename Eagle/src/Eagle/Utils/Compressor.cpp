#include "egpch.h"
#include "Compressor.h"

#include <zstd.h>
#include <zstd_errors.h>

namespace Eagle::Compressor
{
	static constexpr int s_FastLevel = 1;
	static constexpr int s_DefaultLevel = 10;

	namespace
	{
		// Creating a zstd context is relatively expensive (it allocates internal tables),
		// so one context per thread is created lazily and reused for every call.
		struct CCtxDeleter { void operator()(ZSTD_CCtx* ctx) const { ZSTD_freeCCtx(ctx); } };
		struct DCtxDeleter { void operator()(ZSTD_DCtx* ctx) const { ZSTD_freeDCtx(ctx); } };

		ZSTD_CCtx* GetCCtx()
		{
			thread_local std::unique_ptr<ZSTD_CCtx, CCtxDeleter> ctx(ZSTD_createCCtx());
			return ctx.get();
		}

		ZSTD_DCtx* GetDCtx()
		{
			thread_local std::unique_ptr<ZSTD_DCtx, DCtxDeleter> ctx(ZSTD_createDCtx());
			return ctx.get();
		}

		// Returns compressed size or a zstd error code
		size_t CompressImpl(DataBuffer src, void* dst, size_t dstCapacity, int level)
		{
			ZSTD_CCtx* ctx = GetCCtx();
			if (!ctx)
				return size_t(-ZSTD_error_memory_allocation);

			ZSTD_CCtx_reset(ctx, ZSTD_reset_session_and_parameters);
			ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
			ZSTD_CCtx_setParameter(ctx, ZSTD_c_checksumFlag, 1);    // Detect corruption on decompression
			ZSTD_CCtx_setParameter(ctx, ZSTD_c_contentSizeFlag, 1); // Store original size in the frame header
			return ZSTD_compress2(ctx, dst, dstCapacity, src.Data, src.Size);
		}

		// Returns decompressed size or a zstd error code
		size_t DecompressImpl(DataBuffer src, void* dst, size_t dstCapacity)
		{
			ZSTD_DCtx* ctx = GetDCtx();
			if (!ctx)
				return size_t(-ZSTD_error_memory_allocation);

			return ZSTD_decompressDCtx(ctx, dst, dstCapacity, src.Data, src.Size);
		}
	}

	ScopedDataBuffer Compress(DataBuffer data)
	{
		const size_t compressedBound = ZSTD_compressBound(data.Size);
		ScopedDataBuffer result(compressedBound);

		const size_t actualSize = CompressImpl(data, result.Data(), result.Size(), s_DefaultLevel);
		if (ZSTD_isError(actualSize))
		{
			// Failed to compress. Return null
			EG_CORE_ERROR("Failed to compress data. {}", ZSTD_getErrorName(actualSize));
			return {};
		}
		result.GetDataBuffer().Size = actualSize;
		return result;
	}

	size_t CompressFast(DataBuffer data, void* dst, size_t dstCapacity)
	{
		const size_t compressedBound = ZSTD_compressBound(data.Size);
		if (dstCapacity < compressedBound)
		{
			EG_CORE_ERROR("Failed to compress. `Dst` buffer is too small ({} bytes). Required {} bytes", dstCapacity, compressedBound);
			return 0;
		}

		const size_t actualSize = CompressImpl(data, dst, dstCapacity, s_FastLevel);
		if (ZSTD_isError(actualSize))
		{
			EG_CORE_ERROR("Failed to compress data. {}", ZSTD_getErrorName(actualSize));
			return 0;
		}
		return actualSize;
	}

	ScopedDataBuffer Decompress(DataBuffer data)
	{
		if (data.Data == nullptr || data.Size == 0)
		{
			EG_CORE_ERROR("Failed to decompress data. Input is empty");
			return {};
		}

		auto frameSize = ZSTD_getFrameContentSize(data.Data, data.Size);
		if (frameSize == ZSTD_CONTENTSIZE_ERROR || frameSize == ZSTD_CONTENTSIZE_UNKNOWN)
		{
			EG_CORE_ERROR("Failed to decompress data. Input is not a valid zstd frame");
			return {};
		}

		ScopedDataBuffer decompressed(frameSize);
		const size_t written = DecompressImpl(data, decompressed.Data(), decompressed.Size());
		if (ZSTD_isError(written))
		{
			EG_CORE_ERROR("Failed to decompress data. {}", ZSTD_getErrorName(written));
			return {};
		}
		return decompressed;
	}

	size_t Decompress(DataBuffer data, void* dst, size_t dstCapacity)
	{
		if (data.Data == nullptr || data.Size == 0)
		{
			EG_CORE_ERROR("Failed to decompress data. Input is empty");
			return 0;
		}

		auto frameSize = ZSTD_getFrameContentSize(data.Data, data.Size);
		if (frameSize == ZSTD_CONTENTSIZE_ERROR)
		{
			EG_CORE_ERROR("Failed to decompress data. Input is not a valid zstd frame");
			return 0;
		}
		if (dstCapacity < frameSize)
		{
			EG_CORE_ERROR("Failed to decompress. `Dst` buffer is too small ({} bytes). Required {} bytes", dstCapacity, frameSize);
			return 0;
		}

		const size_t written = DecompressImpl(data, dst, dstCapacity);
		if (ZSTD_isError(written))
		{
			EG_CORE_ERROR("Failed to decompress data. {}", ZSTD_getErrorName(written));
			return 0;
		}
		return written;
	}
	
	bool Validate(DataBuffer originalBuffer, DataBuffer decompressedBuffer)
	{
		if (originalBuffer.Size != decompressedBuffer.Size)
			return false;
		if (originalBuffer.Size == 0)
			return true;

		return memcmp(originalBuffer.Data, decompressedBuffer.Data, originalBuffer.Size) == 0;
	}
}
