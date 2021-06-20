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
}
