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
		
		const auto& rendererOptions = m_Editor->GetEditorState() == EditorState::Play ? m_Editor->m_RendererSettingsBeforePlay : m_Editor->m_CurrentScene->GetSceneRenderer()->GetOptions();
		const auto& bloomSettings = rendererOptions.BloomSettings;
		const auto& ssaoSettings = rendererOptions.SSAOSettings;
		const auto& gtaoSettings = rendererOptions.GTAOSettings;
		const auto& fogSettings = rendererOptions.FogSettings;
		const auto& volumetricSettings = rendererOptions.VolumetricSettings;
		const auto& shadowSettings = rendererOptions.ShadowsSettings;

		out << YAML::Key << "OpenedScenePath" << YAML::Value << openedScenePath.string();
		out << YAML::Key << "WindowSize" << YAML::Value << windowSize;
		out << YAML::Key << "WindowMaximized" << YAML::Value << bWindowMaximized;
		out << YAML::Key << "WindowPos" << YAML::Value << windowPos;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "GuizmoType" << YAML::Value << guizmoType;
		out << YAML::Key << "Style" << YAML::Value << Utils::GetEnumName(m_Editor->m_EditorStyle);
		out << YAML::Key << "VSync" << YAML::Value << bVSync;
		out << YAML::Key << "SoftShadows" << YAML::Value << rendererOptions.bEnableSoftShadows;
		out << YAML::Key << "TranslucentShadows" << YAML::Value << rendererOptions.bTranslucentShadows;
		out << YAML::Key << "ShadowsSmoothTransition" << YAML::Value << rendererOptions.bEnableCSMSmoothTransition;
		out << YAML::Key << "StutterlessShaders" << YAML::Value << rendererOptions.bStutterlessShaders;
		out << YAML::Key << "LineWidth" << YAML::Value << rendererOptions.LineWidth;
		out << YAML::Key << "GridScale" << YAML::Value << rendererOptions.GridScale;
		out << YAML::Key << "TransparencyLayers" << YAML::Value << rendererOptions.TransparencyLayers;
		out << YAML::Key << "AO" << YAML::Value << Utils::GetEnumName(rendererOptions.AO);
		out << YAML::Key << "AA" << YAML::Value << Utils::GetEnumName(rendererOptions.AA);

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
		out << YAML::EndMap; // SSAO Settings

		out << YAML::Key << "GTAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << gtaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << gtaoSettings.GetRadius();
		out << YAML::EndMap; // GTAO Settings

		out << YAML::Key << "Fog Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value << fogSettings.Color;
		out << YAML::Key << "MinDistance" << YAML::Value << fogSettings.MinDistance;
		out << YAML::Key << "MaxDistance" << YAML::Value << fogSettings.MaxDistance;
		out << YAML::Key << "Density" << YAML::Value << fogSettings.Density;
		out << YAML::Key << "Equation" << YAML::Value << Utils::GetEnumName(fogSettings.Equation);
		out << YAML::Key << "bEnable" << YAML::Value << fogSettings.bEnable;
		out << YAML::EndMap; // Fog Settings

		out << YAML::Key << "Volumetric Light Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << volumetricSettings.Samples;
		out << YAML::Key << "MaxScatteringDistance" << YAML::Value << volumetricSettings.MaxScatteringDistance;
		out << YAML::Key << "bFogEnable" << YAML::Value << volumetricSettings.bFogEnable;
		out << YAML::Key << "bEnable" << YAML::Value << volumetricSettings.bEnable;
		out << YAML::EndMap; // Volumetric Light Settings

		out << YAML::Key << "Shadow Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "PointLightSize" << YAML::Value << shadowSettings.PointLightShadowMapSize;
		out << YAML::Key << "SpotLightSize" << YAML::Value << shadowSettings.SpotLightShadowMapSize;
		out << YAML::Key << "DirLightSizes" << YAML::Value << shadowSettings.DirLightShadowMapSizes;
		out << YAML::EndMap; // Shadow Settings

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
		SceneRendererSettings settings;

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
			m_Editor->m_EditorStyle = Utils::GetEnumFromName<ImGuiLayer::Style>(styleNode.as<std::string>());
		if (auto VSyncNode = data["VSync"])
			m_Editor->m_VSync = VSyncNode.as<bool>();
		if (auto softShadows = data["SoftShadows"])
			settings.bEnableSoftShadows = softShadows.as<bool>();
		if (auto translucentShadows = data["TranslucentShadows"])
			settings.bTranslucentShadows = translucentShadows.as<bool>();
		if (auto smoothShadows = data["ShadowsSmoothTransition"])
			settings.bEnableCSMSmoothTransition = smoothShadows.as<bool>();
		if (auto stutterless = data["StutterlessShaders"])
			settings.bStutterlessShaders = stutterless.as<bool>();
		if (auto lineWidthNode = data["LineWidth"])
			settings.LineWidth = lineWidthNode.as<float>();
		if (auto gridScaleNode = data["GridScale"])
			settings.GridScale = gridScaleNode.as<float>();
		if (auto layersNode = data["TransparencyLayers"])
			settings.TransparencyLayers = layersNode.as<uint32_t>();
		if (auto node = data["AO"])
			settings.AO = Utils::GetEnumFromName<AmbientOcclusion>(node.as<std::string>());
		if (auto node = data["AA"])
			settings.AA = Utils::GetEnumFromName<AAMethod>(node.as<std::string>());

		if (auto bloomSettingsNode = data["Bloom Settings"])
		{
			settings.BloomSettings.Threshold = bloomSettingsNode["Threshold"].as<float>();
			settings.BloomSettings.Intensity = bloomSettingsNode["Intensity"].as<float>();
			settings.BloomSettings.DirtIntensity = bloomSettingsNode["DirtIntensity"].as<float>();
			settings.BloomSettings.Knee = bloomSettingsNode["Knee"].as<float>();
			settings.BloomSettings.bEnable = bloomSettingsNode["bEnable"].as<bool>();
			Serializer::DeserializeTexture2D(bloomSettingsNode, settings.BloomSettings.Dirt, "Dirt");
		}

		if (auto ssaoSettingsNode = data["SSAO Settings"])
		{
			settings.SSAOSettings.SetNumberOfSamples(ssaoSettingsNode["Samples"].as<uint32_t>());
			settings.SSAOSettings.SetRadius(ssaoSettingsNode["Radius"].as<float>());
			settings.SSAOSettings.SetBias(ssaoSettingsNode["Bias"].as<float>());
		}

		if (auto gtaoSettingsNode = data["GTAO Settings"])
		{
			settings.GTAOSettings.SetNumberOfSamples(gtaoSettingsNode["Samples"].as<uint32_t>());
			settings.GTAOSettings.SetRadius(gtaoSettingsNode["Radius"].as<float>());
		}

		if (auto fogSettingsNode = data["Fog Settings"])
		{
			settings.FogSettings.Color = fogSettingsNode["Color"].as<glm::vec3>();
			settings.FogSettings.MinDistance = fogSettingsNode["MinDistance"].as<float>();
			settings.FogSettings.MaxDistance = fogSettingsNode["MaxDistance"].as<float>();
			settings.FogSettings.Density = fogSettingsNode["Density"].as<float>();
			settings.FogSettings.Equation = Utils::GetEnumFromName<FogEquation>(fogSettingsNode["Equation"].as<std::string>());
			settings.FogSettings.bEnable = fogSettingsNode["bEnable"].as<bool>();
		}

		if (auto volumetricSettingsNode = data["Volumetric Light Settings"])
		{
			settings.VolumetricSettings.Samples = volumetricSettingsNode["Samples"].as<uint32_t>();
			settings.VolumetricSettings.MaxScatteringDistance = volumetricSettingsNode["MaxScatteringDistance"].as<float>();
			settings.VolumetricSettings.bFogEnable = volumetricSettingsNode["bFogEnable"].as<bool>();
			settings.VolumetricSettings.bEnable = volumetricSettingsNode["bEnable"].as<bool>();
		}

		if (auto shadowSettingsNode = data["Shadow Settings"])
		{
			settings.ShadowsSettings.PointLightShadowMapSize = shadowSettingsNode["PointLightSize"].as<uint32_t>();
			settings.ShadowsSettings.SpotLightShadowMapSize = shadowSettingsNode["SpotLightSize"].as<uint32_t>();
			settings.ShadowsSettings.DirLightShadowMapSizes = shadowSettingsNode["DirLightSizes"].as<std::vector<uint32_t>>();
		}

		m_Editor->OnDeserialized(windowSize, windowPos, settings, bWindowMaximized);
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
