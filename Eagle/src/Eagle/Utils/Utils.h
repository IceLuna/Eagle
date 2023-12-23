#pragma once

#include "Eagle/Core/Log.h"

#include <map>
#include <magic_enum.hpp>

namespace Eagle::Utils
{
	enum class FileFormat : uint8_t
	{
		Unknown,
		Texture,
		TextureCube,
		Mesh,
		Scene,
		Sound,
		Font,
	};

	//Just add a new format here
	const std::unordered_map<std::string, FileFormat> SupportedFileFormats = 
	{
		{ ".png",   FileFormat::Texture },
		{ ".jpg",   FileFormat::Texture },
		{ ".tga",   FileFormat::Texture },
		{ ".hdr",   FileFormat::TextureCube },
		{ ".eagle", FileFormat::Scene },
		{ ".fbx",   FileFormat::Mesh },
		{ ".blend", FileFormat::Mesh },
		{ ".3ds",   FileFormat::Mesh },
		{ ".obj",   FileFormat::Mesh },
		{ ".smd",   FileFormat::Mesh },
		{ ".vta",   FileFormat::Mesh },
		{ ".stl",   FileFormat::Mesh },
		{ ".wav",   FileFormat::Sound },
		{ ".ogg",   FileFormat::Sound },
		{ ".wma",   FileFormat::Sound },
		{ ".ttf",   FileFormat::Font },
		{ ".otf",   FileFormat::Font },
	};

	FileFormat GetSupportedFileFormat(const Path& filepath);

	std::string ToUtf8(const std::wstring& str);

	size_t FindSubstringI(const std::string& str1, const std::string& str2);
	size_t FindSubstringI(const std::wstring& str1, const std::wstring& str2);

	template<typename Enum>
	const char* GetEnumName(Enum value)
	{
		return magic_enum::enum_name(value).data();
	}

	template<typename Enum>
	Enum GetEnumFromName(const std::string& name)
	{
		auto value = magic_enum::enum_cast<Enum>(name);
		if (value.has_value()) {
			return value.value();
		}

		EG_EDITOR_WARN("Couldn't get enum from name: {}", name);
		return Enum();
	}
}