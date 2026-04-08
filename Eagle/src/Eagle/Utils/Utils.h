#pragma once

#include "Eagle/Core/Log.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Renderer/RendererUtils.h"

#include <map>
#include <magic_enum.hpp>
#include <glm/glm.hpp>

namespace Eagle
{
	class StaticMesh;
	class SkeletalMesh;
	class AssetMaterial;
	struct SkeletalMeshAnimation;
}

namespace Eagle::Utils
{
	struct StaticMeshImportData
	{
		Ref<StaticMesh> Mesh;
		std::vector<uint32_t> MaterialIndices; // Indices of imported materials
	};

	struct SkeletalMeshImportData
	{
		Ref<SkeletalMesh> Mesh;
		std::vector<uint32_t> MaterialIndices; // Indices of imported materials
	};

	std::string ToUtf8(const std::wstring& str);
	std::string AsString(const Path& path);
	static Path AsPath(const std::string& str) { return Path{ std::u8string(str.begin(), str.end()) }; }

	static uint16_t ToFloat16(float value)
	{
		return glm::detail::toFloat16(value);
	}

	static float ToFloat32(uint16_t value)
	{
		return glm::detail::toFloat32(value);
	}

	// Function to convert float32 to unsigned float10
	static uint16_t ToFloat10(float value)
	{
		uint32_t float_bits;
		std::memcpy(&float_bits, &value, sizeof(float_bits));

		uint32_t sign = (float_bits >> 31) & 0x1;
		uint32_t exponent = (float_bits >> 23) & 0xFF;
		uint32_t significand = float_bits & 0x7FFFFF;

		uint16_t unsigned_float10_exponent;
		uint16_t unsigned_float10_significand;

		if (exponent == 0) {
			unsigned_float10_exponent = 0;
			unsigned_float10_significand = significand >> 18;
		}
		else {
			unsigned_float10_exponent = exponent - 127 + 15;  // Adjusting bias from float32 (127) to float10 (15)
			unsigned_float10_significand = significand >> 18;
		}

		return (unsigned_float10_exponent << 5) | (unsigned_float10_significand & 0x1F);
	}

	// Function to convert float32 to unsigned float11
	static uint16_t ToFloat11(float value)
	{
		uint32_t float_bits;
		std::memcpy(&float_bits, &value, sizeof(float_bits));

		uint32_t sign = (float_bits >> 31) & 0x1;
		uint32_t exponent = (float_bits >> 23) & 0xFF;
		uint32_t significand = float_bits & 0x7FFFFF;

		uint16_t unsigned_float11_exponent;
		uint16_t unsigned_float11_significand;

		if (exponent == 0) {
			unsigned_float11_exponent = 0;
			unsigned_float11_significand = significand >> 17;
		}
		else {
			unsigned_float11_exponent = exponent - 127 + 15;  // Adjusting bias from float32 (127) to float11 (15)
			unsigned_float11_significand = significand >> 17;
		}

		return (unsigned_float11_exponent << 6) | (unsigned_float11_significand & 0x3F);
	}

	// Function to pack float32 RGB values into a uint32_t
	static uint32_t ToR11G11B10(glm::vec3 rgb)
	{
		// Discarding some mantissa bits.
		// Note: shifting to the right needs to be "4 or 5" instead of "5 or 6"
		// because float16 has a sign bit and we don't need it.
		uint16_t r11 = (Utils::ToFloat16(rgb.r) >> 4) & 0x7FF;
		uint16_t g11 = (Utils::ToFloat16(rgb.g) >> 4) & 0x7FF;
		uint16_t b10 = (Utils::ToFloat16(rgb.b) >> 5) & 0x3FF;
		return (b10 << 22) | (g11 << 11) | r11;
	}

	size_t FindSubstringI(const std::string& str1, const std::string& str2);
	size_t FindSubstringI(const std::wstring& str1, const std::wstring& str2);

	// It's a guess, not a 100% answer
	bool IsNormalMap(const Path& path);
	bool IsNormalMap(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);

	// @width. Returns the width of the texture
	// @height. Returns the height of the texture
	// @channels. Returns number of channels in a file/buffer.
	// @desiredNumChannels. Output data will contain `desiredNumChannels` channels. Can be set to 0 to avoid conversion
	ScopedDataBuffer LoadTextureFromFile(const Path& path, int* width, int* height, int* channels, uint32_t desiredNumChannels);
	ScopedDataBuffer LoadTextureFromMemory(const DataBuffer& buffer, int* width, int* height, int* channels, uint32_t desiredNumChannels);
	inline ScopedDataBuffer LoadTextureFromMemory(const ScopedDataBuffer& buffer, int* width, int* height, int* channels, uint32_t desiredNumChannels)
	{
		return LoadTextureFromMemory(buffer.GetDataBuffer(), width, height, channels, desiredNumChannels);
	}

	// @width. Returns the width of the texture
	// @height. Returns the height of the texture
	// @channels. Returns number of channels in a file/buffer.
	// @desiredFormat. Desired format of the output(binary) value. Supported formats: RGBA32F, RGBA16F, R11G11B10_Float
	ScopedDataBuffer LoadHDRTextureFromMemory(const DataBuffer& buffer, int* width, int* height, int* channels, ImageFormat desiredFormat = ImageFormat::R32G32B32A32_Float);
	inline ScopedDataBuffer LoadHDRTextureFromMemory(const ScopedDataBuffer& buffer, int* width, int* height, int* channels, ImageFormat desiredFormat = ImageFormat::R32G32B32A32_Float)
	{
		return LoadHDRTextureFromMemory(buffer.GetDataBuffer(), width, height, channels, desiredFormat);
	}

	ScopedDataBuffer ToPNG(DataBuffer imageData, glm::uvec2 size, uint32_t numChannels);

	template<typename Enum>
	const char* GetEnumName(Enum value)
	{
		return magic_enum::enum_name(value).data();
	}

	template<typename Enum>
	Enum GetEnumFromName(const std::string_view& name)
	{
		auto value = magic_enum::enum_cast<Enum>(name);
		if (value.has_value())
		{
			return value.value();
		}

		EG_CORE_WARN("Couldn't get enum from name: {}", name);
		return Enum();
	}

	static bool HasExtension(const Path& filepath, const char* extension)
	{
		if (!filepath.has_extension())
			return false;

		static const std::locale& loc = std::locale("RU_ru");
		std::string fileExtension = Utils::AsString(filepath.extension());

		for (char& c : fileExtension)
			c = std::tolower(c, loc);

		return fileExtension == extension;
	}

	// @saveTo. Folder to save to.
	// @assetFilename. Asset filename (without the extension)
	Path GetUniqueAssetFilepath(const Path& saveTo, const std::string& assetFilename);
}