#include "egpch.h"

#include "EditorSerializer.h"
#include "EditorLayer.h"
#include <glm/glm.hpp>

namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
}

namespace Eagle
{
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v);
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v);
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v);

	EditorSerializer::EditorSerializer(EditorLayer* editor) : m_Editor(editor)
	{
	}

	bool EditorSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;

		glm::vec3& snapValues = m_Editor->m_SnappingValues;
		int guizmoType = m_Editor->m_GuizmoType;
		bool bVSync = m_Editor->m_VSync;
		bool bInvertColors = m_Editor->m_InvertColors;

		Window& window = Application::Get().GetWindow();
		glm::vec2 windowSize = window.GetWindowSize();
		glm::vec2 windowPos = window.GetWindowPos();

		out << YAML::Key << "WindowSize" << YAML::Value << windowSize;
		out << YAML::Key << "WindowPos" << YAML::Value << windowPos;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "GuizmoType" << YAML::Value << guizmoType;
		out << YAML::Key << "VSync" << YAML::Value << bVSync;
		out << YAML::Key << "InvertColors" << YAML::Value << bInvertColors;
		out << YAML::EndMap;

		std::filesystem::path fs(filepath);
		if (std::filesystem::exists(fs.parent_path()) == false)
		{
			std::filesystem::create_directory(fs.parent_path());
		}
		std::ofstream fout(filepath);
		fout << out.c_str();

		return true;
	}

	bool EditorSerializer::Deserialize(const std::string& filepath)
	{
		SetDefaultValues();
		glm::vec2 windowSize = glm::vec2{ -1, -1 };
		glm::vec2 windowPos = glm::vec2{ -1, -1 };

		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load Editor Preferences {0}. File doesn't exist!", filepath);
			m_Editor->OnDeserialized(windowSize, windowPos);
			return false;
		}

		YAML::Node data = YAML::LoadFile(filepath);

		auto windowSizeNode = data["WindowSize"];
		if (windowSizeNode)
		{
			windowSize = windowSizeNode.as<glm::vec2>();
		}
		auto windowPosNode = data["WindowPos"];
		if (windowPosNode)
		{
			windowPos = windowPosNode.as<glm::vec2>();
		}
		auto snapValuesNode = data["SnapValues"];
		if (snapValuesNode)
		{
			m_Editor->m_SnappingValues = snapValuesNode.as<glm::vec3>();
		}
		auto GuizmoTypeNode = data["GuizmoType"];
		if (GuizmoTypeNode)
		{
			m_Editor->m_GuizmoType = std::max(0, GuizmoTypeNode.as<int>());
		}
		auto VSyncNode = data["VSync"];
		if (VSyncNode)
		{
			m_Editor->m_VSync = VSyncNode.as<bool>();
		}
		auto InvertColorsNode = data["InvertColors"];
		if (InvertColorsNode)
		{
			m_Editor->m_InvertColors = InvertColorsNode.as<bool>();
		}

		m_Editor->OnDeserialized(windowSize, windowPos);
		return true;
	}

	bool EditorSerializer::SerializeBinary(const std::string& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	bool EditorSerializer::DeserializeBinary(const std::string& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	void EditorSerializer::SetDefaultValues()
	{
		m_Editor->m_SnappingValues = glm::vec3{0.1f, 5.f, 0.1f};
		m_Editor->m_GuizmoType = 0;
		m_Editor->m_VSync = true;
		m_Editor->m_InvertColors = false;
	}
}
