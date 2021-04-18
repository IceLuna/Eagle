#pragma once

#include <yaml-cpp/yaml.h>

namespace Eagle
{
	class EditorLayer;

	class EditorSerializer
	{
	public:
		EditorSerializer(EditorLayer* editor);

		bool Serialize(const std::string& filepath);
		bool SerializeBinary(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeBinary(const std::string& filepath);

	private:
		void SetDefaultValues();

	private:
		EditorLayer* m_Editor;
	};
}
