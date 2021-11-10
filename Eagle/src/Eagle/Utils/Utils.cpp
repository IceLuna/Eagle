#include "egpch.h"
#include "Utils.h"

namespace Eagle::Utils
{
	FileFormat GetSupportedFileFormat(const std::filesystem::path& filepath)
	{
		if (!filepath.has_extension())
			return FileFormat::UNKNOWN;

		static const std::locale& loc = std::locale("RU_ru");
		std::string extension = filepath.extension().u8string();

		for (char& c : extension)
			c = std::tolower(c, loc);

		auto it = SupportedFileFormats.find(extension);
		if (it != SupportedFileFormats.end())
			return it->second;

		return FileFormat::UNKNOWN;
	}

	std::string toUtf8(const std::wstring& str)
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
}
