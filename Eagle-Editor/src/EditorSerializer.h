#pragma once

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class EditorLayer;

	class EditorSerializer
	{
	private:
		EditorSerializer() = default;

	public:
		static bool Serialize(EditorLayer* editor, const Path& filepath);
		static bool Deserialize(EditorLayer* editor, const Path& filepath);
	};
}
