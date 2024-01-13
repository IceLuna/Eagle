#pragma once

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class EditorLayer;

	class EditorSerializer
	{
	public:
		EditorSerializer(EditorLayer* editor) : m_Editor(editor) {}

		bool Serialize(const Path& filepath);
		bool Deserialize(const Path& filepath);

	private:
		EditorLayer* m_Editor;
	};
}
