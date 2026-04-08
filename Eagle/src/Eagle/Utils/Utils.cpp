#include "egpch.h"
#include "Utils.h"

#include "Eagle/Asset/Asset.h"

#include <locale>
#include <stb_image.h>
#include <stb_image_write.h>

namespace Eagle
{
	// templated version of my_equal so it could work with both char and wchar_t
	template<typename charT>
	struct my_equal {
		my_equal(const std::locale& loc) : loc_(loc) {}
		bool operator()(charT ch1, charT ch2) {
			return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
		}
	private:
		const std::locale& loc_;
	};

	// find substring (case insensitive)
	template<typename T>
	static size_t MyFindStrTemplate(const T& str1, const T& str2, const std::locale& loc = std::locale("RU_ru"))
	{
		typename T::const_iterator it = std::search(str1.begin(), str1.end(),
			str2.begin(), str2.end(), my_equal<typename T::value_type>(loc));
		if (it != str1.end()) return it - str1.begin();
		else return std::string::npos; // not found
	}

	template <typename T>
	static ScopedDataBuffer ConvertTextureFormat(T* stbiData, uint32_t numPixels, uint32_t inputBPP, uint32_t desiredNumChannels)
	{
		const size_t textureMemSize = uint64_t(numPixels) * desiredNumChannels * sizeof(T);
		ScopedDataBuffer buffer(textureMemSize);
		T* dst = (T*)buffer.Data();

		uint32_t dstIdx = 0;
		for (uint32_t i = 0; i < numPixels; ++i)
		{
			T* base = (T*)stbiData + i * inputBPP;
			T rgba[4] = { 0 };
			for (uint32_t j = 0; j < desiredNumChannels; ++j)
				dst[dstIdx++] = *(base + j);
		}

		return buffer;
	}

	static ScopedDataBuffer ConvertHDRToDesiredFormat(const DataBuffer& buffer, uint32_t numPixels, uint32_t inputBPP, ImageFormat format)
	{
		if (format == ImageFormat::R32G32B32A32_Float)
			return ScopedDataBuffer{ DataBuffer::Copy(buffer) };

		if (format == ImageFormat::R16G16B16A16_Float)
		{
			const size_t pixels = numPixels * inputBPP;

			ScopedDataBuffer imageData(pixels * sizeof(uint16_t));
			uint16_t* dst = (uint16_t*)imageData.Data();
			float* imageData32 = (float*)buffer.Data;

			for (size_t i = 0; i < pixels; ++i)
				dst[i] = Utils::ToFloat16(imageData32[i]);

			return imageData;
		}
		else if (format == ImageFormat::R11G11B10_Float)
		{
			ScopedDataBuffer imageData(numPixels * sizeof(uint32_t));
			uint32_t* dst = (uint32_t*)imageData.Data();
			float* imageData32f = (float*)buffer.Data;
			for (size_t i = 0; i < numPixels; ++i)
			{
				glm::vec3 rgb = glm::vec3(imageData32f[i * 4], imageData32f[i * 4 + 1], imageData32f[i * 4 + 2]);
				dst[i] = Utils::ToR11G11B10(rgb);
			}

			return imageData;
		}

		EG_CORE_ASSERT(!"Unsupported format");
		return ScopedDataBuffer{ DataBuffer::Copy(buffer) };
	}

	static void ToPNGCallback(void* context, void* data, int size)
	{
		ScopedDataBuffer* buffer = (ScopedDataBuffer*)context;
		*buffer = DataBuffer::Copy(data, size);
	}

	std::string Utils::ToUtf8(const std::wstring& str)
	{
		std::string ret;
		int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), &ret[0], len, NULL, NULL);
		}
		return ret;
	}

	std::string Utils::AsString(const Path& path)
	{
		const std::u8string u8str = path.u8string();
		return std::string(u8str.begin(), u8str.end());
	}
	
	size_t Utils::FindSubstringI(const std::string& str1, const std::string& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
	
	size_t Utils::FindSubstringI(const std::wstring& str1, const std::wstring& str2)
	{
		return MyFindStrTemplate(str1, str2);
	}
	
	bool Utils::IsNormalMap(const Path& path)
	{
		char cpath[2048];
		std::wstring wPathString = path.wstring();
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);
		int width, height, channels;
		stbi_info(cpath, &width, &height, &channels);
		if (channels < 3)
			return false;

		const uint32_t desiredChannels = 3;
		uint8_t* imageData = stbi_load(cpath, &width, &height, &channels, desiredChannels);
		if (!imageData)
			return false;

		if (channels < 3)
		{
			stbi_image_free(imageData);
			return false;
		}

		bool bResult = IsNormalMap(imageData, width, height, desiredChannels);
		stbi_image_free(imageData);
		return bResult;
	}
	
	bool Utils::IsNormalMap(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels)
	{
		if (channels != 3)
			return false;

		// Accumulate color values
		uint64_t rTotal = 0, gTotal = 0, bTotal = 0;
		const uint32_t pixelCount = width * height;

		for (uint32_t i = 0; i < pixelCount * channels; i += channels)
		{
			rTotal += data[i + 0];
			gTotal += data[i + 1];
			bTotal += data[i + 2];
		}

		// Calculate the mean RGB values
		double rMean = rTotal / (double)pixelCount;
		double gMean = gTotal / (double)pixelCount;
		double bMean = bTotal / (double)pixelCount;

		// Check if the values are typical of a normal map
		return (rMean >= 120 && rMean <= 136) &&
			(gMean >= 120 && gMean <= 136) &&
			(bMean >= 200 && bMean <= 255);
	}
	
	ScopedDataBuffer Utils::LoadTextureFromFile(const Path& path, int* width, int* height, int* channels, uint32_t desiredNumChannels)
	{
		constexpr int stbiChannels = 4;
		char cpath[2048];
		std::wstring wPathString = path.wstring();
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);
		uint8_t* stbiData = stbi_load(cpath, width, height, channels, stbiChannels);
		if (!stbiData)
		{
			return {};
		}

		const uint32_t numPixels = (*width) * (*height);
		if (desiredNumChannels == 0)
		{
			// No desired number of channels. Use texture's actual number of channels
			desiredNumChannels = *channels;
		}

		if (desiredNumChannels == stbiChannels)
		{
			const size_t textureMemSize = uint64_t(numPixels) * stbiChannels;
			return ScopedDataBuffer(DataBuffer::Copy(stbiData, textureMemSize));
		}

		// We manually convert to the desired number of channels because stbi doesn't do what we expect.
		// See: 'stbi__convert_format' function. For example, it converts `RGBA` to `RG` as `R` being Luminance (combines `RGB` to `R`), and sets `G` to `A`
		ScopedDataBuffer result = ConvertTextureFormat(stbiData, numPixels, stbiChannels, desiredNumChannels);
		stbi_image_free(stbiData);
		return result;
	}

	ScopedDataBuffer Utils::LoadTextureFromMemory(const DataBuffer& buffer, int* width, int* height, int* channels, uint32_t desiredNumChannels)
	{
		constexpr int stbiChannels = 4;
		uint8_t* stbiData = stbi_load_from_memory((const stbi_uc*)buffer.Data, int(buffer.Size), width, height, channels, stbiChannels);
		if (!stbiData)
		{
			return {};
		}

		const uint32_t numPixels = (*width) * (*height);
		if (desiredNumChannels == 0)
		{
			// No desired number of channels. Use texture's actual number of channels
			desiredNumChannels = *channels;
		}

		if (desiredNumChannels == stbiChannels)
		{
			const size_t textureMemSize = uint64_t(numPixels) * stbiChannels * sizeof(uint8_t);
			return ScopedDataBuffer(DataBuffer::Copy(stbiData, textureMemSize));
		}

		// We manually convert to the desired number of channels because stbi doesn't do what we expect.
		// See: 'stbi__convert_format' function. For example, it converts `RGBA` to `RG` as `R` being Luminance (combines `RGB` to `R`), and sets `G` to `A`
		ScopedDataBuffer result = ConvertTextureFormat(stbiData, numPixels, stbiChannels, desiredNumChannels);
		stbi_image_free(stbiData);
		return result;
	}

	ScopedDataBuffer Utils::LoadHDRTextureFromMemory(const DataBuffer& buffer, int* width, int* height, int* channels, ImageFormat desiredFormat)
	{
		constexpr int stbiChannels = 4;
		float* stbiData = stbi_loadf_from_memory((uint8_t*)buffer.Data, (int)buffer.Size, width, height, channels, stbiChannels);
		if (!stbiData)
		{
			return {};
		}

		const uint32_t numPixels = (*width) * (*height);
		const size_t textureMemSize = uint64_t(numPixels) * stbiChannels * sizeof(float);

		ScopedDataBuffer result = ConvertHDRToDesiredFormat(DataBuffer(stbiData, textureMemSize), numPixels, stbiChannels, desiredFormat);
		stbi_image_free(stbiData);
		return result;
	}

	ScopedDataBuffer Utils::ToPNG(DataBuffer imageData, glm::uvec2 size, uint32_t numChannels)
	{
		const int stride = size.x * numChannels;
		ScopedDataBuffer encoded;
		stbi_write_png_to_func(ToPNGCallback, &encoded, size.x, size.y, numChannels, imageData.Data, stride);
		return encoded;
	}
	
	Path Utils::GetUniqueAssetFilepath(const Path& saveTo, const std::string& assetFilename)
	{
		Path outputFilename = saveTo / Utils::AsPath(assetFilename + Asset::GetExtension());
		uint32_t i = 0;
		while (std::filesystem::exists(outputFilename))
		{
			std::string uniqueFilename = assetFilename + '_' + std::to_string(i);
			outputFilename = saveTo / Utils::AsPath(uniqueFilename + Asset::GetExtension());
			++i;
		}

		return outputFilename;
	}
}
