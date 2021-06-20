#pragma once

namespace Eagle::Utils
{
	enum class FileFormat : uint8_t
	{
		UNKNOWN,
		TEXTURE,
		MESH,
		SCENE
	};

	constexpr std::array<const char*, 2> SupportedTextureExtensions = { ".png", ".jpg" };
	constexpr std::array<const char*, 1> SupportedSceneExtensions = { ".eagle" };
	constexpr std::array<const char*, 7> SupportedMeshExtensions = { ".fbx", ".blend", ".3ds", ".obj", ".smd", ".vta", ".stl" };

	FileFormat GetFileFormat(const std::filesystem::path& filepath);

	std::string toUtf8(const std::wstring& str);
}