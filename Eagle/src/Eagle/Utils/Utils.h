#pragma once

#include <map>

namespace Eagle::Utils
{
	enum class FileFormat : uint8_t
	{
		UNKNOWN,
		TEXTURE,
		MESH,
		SCENE
	};

	//Just add a new format here
	const std::map<std::string, FileFormat> SupportedFileFormats = 
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
	};

	FileFormat GetFileFormat(const std::filesystem::path& filepath);

	std::string toUtf8(const std::wstring& str);
}