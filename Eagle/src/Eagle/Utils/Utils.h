#pragma once

#include <map>

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
}