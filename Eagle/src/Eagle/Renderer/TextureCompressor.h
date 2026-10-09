#pragma once

#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <glm/glm.hpp>

namespace Eagle
{
	class Image;
	class CommandBuffer;

	class TextureCompressor
	{
	public:
		// Receives compression processing progress as a percentage in [0; 1] range
		// Callback need to keep returnting `true` to continue compression.
		// Note: callback is called on the caller's thread, and it's called regularly (~30 times per second) even if the progress doesn't change
		using ProgressCallback = std::function<bool(float)>;

		enum class Quality
		{
			Disabled,
			Medium, // BC1, BC3
			High, // BC7
		};

		struct Result
		{
			std::vector<ScopedDataBuffer> DataPerMip; // Contains compressed texture data ready to be uploaded to the GPU
			ImageFormat Format = ImageFormat::Unknown;

			bool IsValid() const
			{
				return !DataPerMip.empty() && Format != ImageFormat::Unknown;
			}

			operator bool() const
			{
				return IsValid();
			}
		};

		enum class TextureType
		{
			Regular, // R/RG/RGB
			RegularWithAlpha, // RGBA
			NormalMap,
			HDR,
		};

		static void Init();
		static void Shutdown();

		static bool IsCompressionFormatSupported(ImageFormat format);

		// @imageData. Input texture data that needs to be decoded and compressed. It needs be a data that's read from a file (for example, .png)
		// @targetNumChannels. Number of channels in the compressed textures. Used to determine compression algorithm (BC1/3/4/5 etc)
		// @mipsCount. Should be >= 1. The value of `1` represents the base level. So if it's 1, mips won't be generated
		// @bNormalMap. Set to true, if it's a normal map
		// @progress. Optional callback reporting compression progress
		// @return. Compressed data per mip and the format
		static Result Compress(DataBuffer imageData, uint32_t targetNumChannels, uint32_t mipsCount, Quality quality, bool bNormalMap, bool bHDR = false, ProgressCallback progress = {});

		// Same, but.
		// @imageData. Input texture data in RGBA8 format
		// @size. Texture size
		// @progress. Optional callback reporting compression progress
		static Result CompressDecoded(DataBuffer imageData, glm::uvec2 size, uint32_t targetNumChannels, uint32_t mipsCount, Quality quality, bool bNormalMap, bool bHDR = false, ProgressCallback progress = {});

		// This version of the function performs BC6H compression on the GPU.
		// @imageData. Input texture data that needs to be compressed
		// @size. Texture size of `imageData`
		// @format. Image format of `imageData`
		// @dst. The destination texture. Needs to have `BC6H_UFloat` format
		// @return. True on success
		static bool CompressHDR(const void* imageData, glm::uvec2 size, ImageFormat format, const Ref<Image>& dst);

		// This version of the function performs BC6H compression on the GPU. Supports compression of cube images. Supports mips generation
		// @cmd. Command buffer to use for recording
		// @src. Source texture
		// @return. On success, returns a valid BC6H image
		static Ref<Image> CompressHDR(const Ref<CommandBuffer>& cmd, const Ref<Image>& src);
	};
}
