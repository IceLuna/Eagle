#include "egpch.h"

#include "Eagle/Core/Serializer.h"

#include "EditorSerializer.h"
#include "EditorLayer.h"

namespace Eagle
{
	EditorSerializer::EditorSerializer(EditorLayer* editor) : m_Editor(editor)
	{
	}

	bool EditorSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;

		std::filesystem::path openedScenePath = m_Editor->m_OpenedScenePath;

		std::filesystem::path currentPath = std::filesystem::current_path();
		std::filesystem::path relPath = std::filesystem::relative(openedScenePath, currentPath);

		if (!relPath.empty())
			openedScenePath = relPath;

		const glm::vec3& snapValues = m_Editor->m_SnappingValues;
		int guizmoType = m_Editor->m_GuizmoType;
		bool bVSync = m_Editor->m_VSync;

		const Window& window = Application::Get().GetWindow();
		glm::vec2 windowSize = window.GetWindowSize();
		bool bWindowMaximized = window.IsMaximized();
		glm::vec2 windowPos = window.GetWindowPos();
		
		const auto& rendererOptions = m_Editor->m_CurrentScene->GetSceneRenderer()->GetOptions();
		const auto& bloomSettings = rendererOptions.BloomSettings;
		const auto& ssaoSettings = rendererOptions.SSAOSettings;
		const auto& fogSettings = rendererOptions.FogSettings;

		out << YAML::Key << "OpenedScenePath" << YAML::Value << openedScenePath.string();
		out << YAML::Key << "WindowSize" << YAML::Value << windowSize;
		out << YAML::Key << "WindowMaximized" << YAML::Value << bWindowMaximized;
		out << YAML::Key << "WindowPos" << YAML::Value << windowPos;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "GuizmoType" << YAML::Value << guizmoType;
		out << YAML::Key << "Style" << YAML::Value << m_Editor->m_EditorStyleIdx;
		out << YAML::Key << "VSync" << YAML::Value << bVSync;
		out << YAML::Key << "SoftShadows" << YAML::Value << rendererOptions.bEnableSoftShadows;
		out << YAML::Key << "LineWidth" << YAML::Value << rendererOptions.LineWidth;

		out << YAML::Key << "Bloom Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Threshold" << YAML::Value << bloomSettings.Threshold;
		out << YAML::Key << "Intensity" << YAML::Value << bloomSettings.Intensity;
		out << YAML::Key << "DirtIntensity" << YAML::Value << bloomSettings.DirtIntensity;
		out << YAML::Key << "Knee" << YAML::Value << bloomSettings.Knee;
		out << YAML::Key << "bEnable" << YAML::Value << bloomSettings.bEnable;
		Serializer::SerializeTexture(out, bloomSettings.Dirt, "Dirt");
		out << YAML::EndMap; // Bloom Settings

		out << YAML::Key << "SSAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << ssaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << ssaoSettings.GetRadius();
		out << YAML::Key << "Bias" << YAML::Value << ssaoSettings.GetBias();
		out << YAML::Key << "bEnable" << YAML::Value << ssaoSettings.bEnable;
		out << YAML::EndMap; // SSAO Settings

		out << YAML::Key << "Fog Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value << fogSettings.Color;
		out << YAML::Key << "MinDistance" << YAML::Value << fogSettings.MinDistance;
		out << YAML::Key << "MaxDistance" << YAML::Value << fogSettings.MaxDistance;
		out << YAML::Key << "Density" << YAML::Value << fogSettings.Density;
		out << YAML::Key << "Equation" << YAML::Value << (uint32_t)fogSettings.Equation;
		out << YAML::Key << "bEnable" << YAML::Value << fogSettings.bEnable;
		out << YAML::EndMap; // Fog Settings

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
		bool bWindowMaximized = true;
		BloomSettings bloomSettings;
		SSAOSettings ssaoSettings;
		FogSettings fogSettings;
		float lineWidth = 2.5f;
		bool bSoftShadows = true;

		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load Editor Preferences {0}. File doesn't exist!", filepath);
			return false;
		}

		YAML::Node data = YAML::LoadFile(filepath);

		if (auto openedScenePathNode = data["OpenedScenePath"])
			m_Editor->m_OpenedScenePath = openedScenePathNode.as<std::string>();
		if (auto windowSizeNode = data["WindowSize"])
			windowSize = windowSizeNode.as<glm::vec2>();
		if (auto windowMaximizedNode = data["WindowMaximized"])
			bWindowMaximized = windowMaximizedNode.as<bool>();
		if (auto windowPosNode = data["WindowPos"])
			windowPos = windowPosNode.as<glm::vec2>();
		if (auto snapValuesNode = data["SnapValues"])
			m_Editor->m_SnappingValues = snapValuesNode.as<glm::vec3>();
		if (auto GuizmoTypeNode = data["GuizmoType"])
			m_Editor->m_GuizmoType = std::max(0, GuizmoTypeNode.as<int>());
		if (auto styleNode = data["Style"])
			m_Editor->m_EditorStyleIdx = std::max(0, styleNode.as<int>());
		if (auto VSyncNode = data["VSync"])
			m_Editor->m_VSync = VSyncNode.as<bool>();
		if (auto softShadows = data["SoftShadows"])
			bSoftShadows = softShadows.as<bool>();
		if (auto lineWidthNode = data["LineWidth"])
			lineWidth = lineWidthNode.as<float>();

		if (auto bloomSettingsNode = data["Bloom Settings"])
		{
			bloomSettings.Threshold = bloomSettingsNode["Threshold"].as<float>();
			bloomSettings.Intensity = bloomSettingsNode["Intensity"].as<float>();
			bloomSettings.DirtIntensity = bloomSettingsNode["DirtIntensity"].as<float>();
			bloomSettings.Knee = bloomSettingsNode["Knee"].as<float>();
			bloomSettings.bEnable = bloomSettingsNode["bEnable"].as<bool>();
			Serializer::DeserializeTexture2D(bloomSettingsNode, bloomSettings.Dirt, "Dirt");
		}

		if (auto ssaoSettingsNode = data["SSAO Settings"])
		{
			ssaoSettings.SetNumberOfSamples(ssaoSettingsNode["Samples"].as<uint32_t>());
			ssaoSettings.SetRadius(ssaoSettingsNode["Radius"].as<float>());
			ssaoSettings.SetBias(ssaoSettingsNode["Bias"].as<float>());
			ssaoSettings.bEnable = ssaoSettingsNode["bEnable"].as<bool>();
		}

		if (auto fogSettingsNode = data["Fog Settings"])
		{
			fogSettings.Color = fogSettingsNode["Color"].as<glm::vec3>();
			fogSettings.MinDistance = fogSettingsNode["MinDistance"].as<float>();
			fogSettings.MaxDistance = fogSettingsNode["MaxDistance"].as<float>();
			fogSettings.Density = fogSettingsNode["Density"].as<float>();
			fogSettings.Equation = (FogEquation)fogSettingsNode["Equation"].as<uint32_t>();
			fogSettings.bEnable = fogSettingsNode["bEnable"].as<bool>();
		}

		m_Editor->OnDeserialized(windowSize, windowPos, bloomSettings, ssaoSettings, fogSettings, lineWidth, bWindowMaximized, bSoftShadows);
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
	}
}
