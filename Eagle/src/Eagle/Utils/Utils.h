#pragma once

#include "Eagle/Core/Log.h"

#include <map>
#include <magic_enum.hpp>
#include <glm/glm.hpp>

namespace Eagle
{
	class StaticMesh;
	class SkeletalMesh;
	class AssetMaterial;
	struct SkeletalMeshAnimation;
}

namespace Eagle::Utils
{
	std::string ToUtf8(const std::wstring& str);

	static uint16_t ToFloat16(float value)
	{
		return glm::detail::toFloat16(value);
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

	Ref<StaticMesh> ImportStaticMesh(const Path& path);

	Ref<SkeletalMesh> ImportSkeletalMesh(const Path& path);

	std::vector<SkeletalMeshAnimation> ImportAnimations(const Path& path, const Ref<SkeletalMesh>& skeletal);

	// Imports materials from a 3D model file
	std::vector<Ref<AssetMaterial>> ImportMaterials(const Path& path, const Path& saveTo);

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