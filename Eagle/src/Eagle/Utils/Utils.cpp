#include "egpch.h"
#include "Utils.h"

namespace Eagle::Utils
{
	FileFormat GetFileFormat(const std::filesystem::path& filepath)
	{
		if (!filepath.has_extension())
			return FileFormat::UNKNOWN;

		const std::string extension = filepath.extension().string();
		const char* cExtension = extension.c_str();

		for (auto& format : SupportedTextureExtensions)
		{
			if (stricmp(format, cExtension) == 0)
				return FileFormat::TEXTURE;
		}

		for (auto& format : SupportedMeshExtensions)
		{
			if (stricmp(format, cExtension) == 0)
				return FileFormat::MESH;
		}

		for (auto& format : SupportedSceneExtensions)
		{
			if (stricmp(format, cExtension) == 0)
				return FileFormat::SCENE;
		}

		return FileFormat::UNKNOWN;
	}

	std::string toUtf8(const std::wstring& str)
	{
		std::string ret;
		int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
		if (len > 0)
		{
			ret.resize(len);
			WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len, NULL, NULL);
		}
		return ret;
	}
}
