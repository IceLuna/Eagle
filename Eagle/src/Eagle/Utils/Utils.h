#pragma once

#include <map>

namespace Eagle::Utils
{
	enum class FileFormat : uint8_t
	{
		UNKNOWN,
		TEXTURE,
		MESH,
		SCENE,
		SOUND
	};

	//Just add a new format here
	const std::unordered_map<std::string, FileFormat> SupportedFileFormats = 
	{
		{ ".png", FileFormat::TEXTURE },
		{ ".jpg", FileFormat::TEXTURE },
		{ ".eagle", FileFormat::SCENE },
		{ ".fbx", FileFormat::MESH },
		{ ".blend", FileFormat::MESH },
		{ ".3ds", FileFormat::MESH },
		{ ".obj", FileFormat::MESH },
		{ ".smd", FileFormat::MESH },
		{ ".vta", FileFormat::MESH },
		{ ".stl", FileFormat::MESH },
		{ ".wav", FileFormat::SOUND },
		{ ".ogg", FileFormat::SOUND },
		{ ".wma", FileFormat::SOUND },
	};

	FileFormat GetSupportedFileFormat(const std::filesystem::path& filepath);

	std::string toUtf8(const std::wstring& str);
}