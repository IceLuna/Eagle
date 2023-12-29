#pragma once

#include "Eagle/Core/Log.h"

#include <map>
#include <magic_enum.hpp>

namespace Eagle::Utils
{
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