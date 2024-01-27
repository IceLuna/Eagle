#include "egpch.h"

#include "Eagle/Core/Serializer.h"
#include "Eagle/Asset/AssetManager.h"

#include "EditorSerializer.h"
#include "EditorLayer.h"

namespace Eagle
{
	bool EditorSerializer::Serialize(const Path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;

		const glm::vec3& snapValues = m_Editor->m_SnappingValues;
		int guizmoType = m_Editor->m_GuizmoType;

		const Window& window = Application::Get().GetWindow();
		glm::vec2 windowSize = window.GetWindowSize();
		bool bWindowMaximized = window.IsMaximized();
		glm::vec2 windowPos = window.GetWindowPos();
		bool bVSync = window.IsVSync();
		const auto& projectInfo = Project::GetProjectInfo();
		
		const auto rendererOptions = m_Editor->GetEditorState() == EditorState::Play ? m_Editor->m_BeforeSimulationData.RendererSettings :
			m_Editor->m_CurrentScene ? m_Editor->m_CurrentScene->GetSceneRenderer()->GetOptions() : SceneRendererSettings{};

		if (m_Editor->m_OpenedSceneAsset)
			out << YAML::Key << "EditorStartupScene" << YAML::Value << m_Editor->m_OpenedSceneAsset->GetGUID();
		if (const auto& scene = projectInfo.GameStartupScene)
			out << YAML::Key << "GameStartupScene" << YAML::Value << scene->GetGUID();
		out << YAML::Key << "ProjectVersion" << YAML::Value << projectInfo.Version;
		out << YAML::Key << "WindowSize" << YAML::Value << windowSize;
		out << YAML::Key << "WindowMaximized" << YAML::Value << bWindowMaximized;
		out << YAML::Key << "WindowPos" << YAML::Value << windowPos;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "GuizmoType" << YAML::Value << guizmoType;
		out << YAML::Key << "Style" << YAML::Value << Utils::GetEnumName(m_Editor->m_EditorStyle);
		out << YAML::Key << "EcoRendering" << YAML::Value << m_Editor->bRenderOnlyWhenFocused;
		out << YAML::Key << "StopSimulationKey" << YAML::Value << Utils::GetEnumName(m_Editor->m_StopSimulationKey);
		out << YAML::Key << "VSync" << YAML::Value << bVSync;

		Serializer::SerializeRendererSettings(out, rendererOptions);

		out << YAML::EndMap;

		if (std::filesystem::exists(filepath.parent_path()) == false)
		{
			std::filesystem::create_directory(filepath.parent_path());
		}
		std::ofstream fout(filepath);
		fout << out.c_str();

		return true;
	}

	bool EditorSerializer::Deserialize(const Path& filepath)
	{
		glm::vec2 windowSize = glm::vec2{ -1, -1 };
		glm::vec2 windowPos = glm::vec2{ -1, -1 };
		bool bWindowMaximized = true;
		SceneRendererSettings settings;

		if (!std::filesystem::exists(filepath))
			return false;

		YAML::Node data = YAML::LoadFile(filepath.string());
		bool bVSync = true;
		bool bRenderOnlyWhenFocused = m_Editor->bRenderOnlyWhenFocused;
		Key stopSimulationKey = m_Editor->m_StopSimulationKey;

		if (auto openedScenePathNode = data["EditorStartupScene"])
		{
			const GUID sceneGUID = openedScenePathNode.as<GUID>();
			Ref<Asset> asset;
			if (AssetManager::Get(sceneGUID, &asset))
			{
				if (Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset))
					m_Editor->m_OpenedSceneAsset = sceneAsset;
			}
		}
		if (auto gameScenePathNode = data["GameStartupScene"])
		{
			const GUID sceneGUID = gameScenePathNode.as<GUID>();
			Ref<Asset> asset;
			if (AssetManager::Get(sceneGUID, &asset))
			{
				if (Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset))
					Project::SetStartupScene(sceneAsset);
			}
		}
		if (auto projectVersionNode = data["ProjectVersion"])
			Project::SetVersion(projectVersionNode.as<glm::uvec3>());
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
		if (auto node = data["EcoRendering"])
			bRenderOnlyWhenFocused = node.as<bool>();
		if (auto node = data["StopSimulationKey"])
			stopSimulationKey = Utils::GetEnumFromName<Eagle::Key>(node.as<std::string>());
		if (auto VSyncNode = data["VSync"])
			bVSync = VSyncNode.as<bool>();
		
		Serializer::DeserializeRendererSettings(data, settings);

		m_Editor->OnDeserialized(windowSize, windowPos, settings, bWindowMaximized, bVSync, bRenderOnlyWhenFocused, stopSimulationKey);
		return true;
	}
}
