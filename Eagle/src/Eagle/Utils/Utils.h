#pragma once

#include "Eagle/Core/Log.h"

#include <map>
#include <magic_enum.hpp>

namespace Eagle
{
	class StaticMesh;
}

namespace Eagle::Utils
{
	std::string ToUtf8(const std::wstring& str);

	static uint16_t ToFloat16(float value)
	{
		uint32_t fltInt32;
		uint16_t fltInt16;

		memcpy(&fltInt32, &value, sizeof(float));

		fltInt16 = (fltInt32 >> 31) << 5;
		unsigned short tmp = (fltInt32 >> 23) & 0xff;
		tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
		fltInt16 = (fltInt16 | tmp) << 10;
		fltInt16 |= (fltInt32 >> 13) & 0x3ff;

		return fltInt16;
	}

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

		EG_CORE_WARN("Couldn't get enum from name: {}", name);
		return Enum();
	}

	std::vector<Ref<StaticMesh>> ImportMeshes(const Path& path);

	static bool HasExtension(const Path& filepath, const char* extension)
	{
		if (!filepath.has_extension())
			return false;

		static const std::locale& loc = std::locale("RU_ru");
		std::string fileExtension = filepath.extension().u8string();

		for (char& c : fileExtension)
			c = std::tolower(c, loc);

		return fileExtension == extension;
	}
}