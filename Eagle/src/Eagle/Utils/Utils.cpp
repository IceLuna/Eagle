#include "egpch.h"
#include "Utils.h"

#include <locale>
#include <stb_image.h>

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
		if (channels != 3)
			return false;

		const uint32_t desiredChannels = 3;
		uint8_t* imageData = stbi_load(cpath, &width, &height, &channels, desiredChannels);
		if (!imageData)
			return false;

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
	
	uint8_t* Utils::LoadTextureFromFile(const Path& path, int* width, int* height, int* channels, uint32_t desiredNumChannels)
	{
		char cpath[2048];
		std::wstring wPathString = path.wstring();
		WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);
		uint8_t* imageData = stbi_load(cpath, width, height, channels, desiredNumChannels);
		if (!imageData)
		{
			return nullptr;
		}
		return imageData;
	}
	
	void Utils::FreeTextureData(void* data)
	{
		stbi_image_free(data);
	}
}
