#include "egpch.h"

#include "Eagle/Core/Serializer.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Utils/PlatformUtils.h"

#include "EditorSerializer.h"
#include "EditorLayer.h"
#include <ImGuizmo/ImGuizmo.h>

namespace Eagle
{
	bool EditorSerializer::Serialize(EditorLayer* editor, const Path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;

		const glm::vec3& snapValues = editor->m_SnappingValues;
		int guizmoType = editor->m_GuizmoType;

		const Window& window = Application::Get().GetWindow();
		glm::vec2 windowSize = window.GetWindowSize();
		bool bWindowMaximized = window.IsMaximized();
		glm::vec2 windowPos = window.GetWindowPos();
		bool bVSync = window.IsVSync();
		
		const auto rendererOptions = editor->GetEditorState() == EditorState::Play ? editor->m_BeforeSimulationData.RendererSettings :
			editor->m_CurrentScene ? editor->m_CurrentScene->GetSceneRenderer()->GetOptions() : SceneRendererSettings{};

		if (editor->m_OpenedSceneAsset)
			out << YAML::Key << "EditorStartupScene" << YAML::Value << editor->m_OpenedSceneAsset->GetGUID();
		out << YAML::Key << "WindowSize" << YAML::Value << windowSize;
		out << YAML::Key << "WindowMaximized" << YAML::Value << bWindowMaximized;
		out << YAML::Key << "WindowPos" << YAML::Value << windowPos;
		out << YAML::Key << "SnapValues" << YAML::Value << snapValues;
		out << YAML::Key << "GuizmoType" << YAML::Value << guizmoType;
		out << YAML::Key << "Style" << YAML::Value << Utils::GetEnumName(editor->m_EditorStyle);
		out << YAML::Key << "EcoRendering" << YAML::Value << editor->bRenderOnlyWhenFocused;
		out << YAML::Key << "bUpdateAnimationsInEditor" << YAML::Value << editor->bUpdateAnimationsInEditor;
		out << YAML::Key << "DrawAxisGuizmo" << YAML::Value << editor->bDrawAxisGuizmo;
		out << YAML::Key << "DrawNavMesh" << YAML::Value << editor->bDrawNavMesh;
		out << YAML::Key << "DrawMeshAABBs" << YAML::Value << editor->bDrawMeshAABBs;
		out << YAML::Key << "StopSimulationKey" << YAML::Value << Utils::GetEnumName(editor->m_StopSimulationKey);
		out << YAML::Key << "VSync" << YAML::Value << bVSync;
		out << YAML::Key << "GuizmoMode" << YAML::Value << Utils::GetEnumName((ImGuizmo::MODE)editor->m_GuizmoMode);

		// Editor camera
		{
			// Location & Rotation aren't saved. They're saved in the scene asset

			const EditorCamera& camera = editor->m_Camera;
			const auto& transform = camera.GetTransform();
			out << YAML::Key << "EditorCamera" << YAML::BeginMap;
			out << YAML::Key << "ProjectionMode" << YAML::Value << Utils::GetEnumName(camera.GetProjectionMode());
			out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "ShadowFarClip" << YAML::Value << camera.GetShadowFarClip();
			out << YAML::Key << "DirLightShadowFarClip" << YAML::Value << camera.GetDirLightShadowFarClip();
			out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();
			out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
			out << YAML::Key << "MoveSpeed" << YAML::Value << camera.GetMoveSpeed();
			out << YAML::Key << "RotationSpeed" << YAML::Value << camera.GetRotationSpeed();
			out << YAML::EndMap;
		}

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

	bool EditorSerializer::Deserialize(EditorLayer* editor, const Path& filepath)
	{
		glm::vec2 windowSize = glm::vec2{ -1, -1 };
		glm::vec2 windowPos = glm::vec2{ -1, -1 };
		bool bWindowMaximized = true;
		SceneRendererSettings settings;

		if (!std::filesystem::exists(filepath))
			return false;

		YAML::Node data = YAML::Load(FileSystem::ReadText(filepath));
		bool bVSync = true;
		bool bRenderOnlyWhenFocused = editor->bRenderOnlyWhenFocused;
		bool bUpdateAnimationsInEditor = editor->bUpdateAnimationsInEditor;
		bool bDrawAxisGuizmo = editor->bDrawAxisGuizmo;
		bool bDrawNavMesh = editor->bDrawNavMesh;
		bool bDrawMeshAABBs = editor->bDrawMeshAABBs;
		Key stopSimulationKey = editor->m_StopSimulationKey;
		int guizmoMode = ImGuizmo::MODE::WORLD;
		EditorCamera camera{};

		if (auto openedScenePathNode = data["EditorStartupScene"])
		{
			const GUID sceneGUID = openedScenePathNode.as<GUID>();
			Ref<Asset> asset;
			if (AssetManager::Get(sceneGUID, &asset))
			{
				if (Ref<AssetScene> sceneAsset = Cast<AssetScene>(asset))
					editor->m_OpenedSceneAsset = sceneAsset;
			}
		}
		if (auto windowSizeNode = data["WindowSize"])
			windowSize = windowSizeNode.as<glm::vec2>();
		if (auto windowMaximizedNode = data["WindowMaximized"])
			bWindowMaximized = windowMaximizedNode.as<bool>();
		if (auto windowPosNode = data["WindowPos"])
			windowPos = windowPosNode.as<glm::vec2>();
		if (auto snapValuesNode = data["SnapValues"])
			editor->m_SnappingValues = snapValuesNode.as<glm::vec3>();
		if (auto GuizmoTypeNode = data["GuizmoType"])
			editor->m_GuizmoType = std::max(0, GuizmoTypeNode.as<int>());
		if (auto styleNode = data["Style"])
			editor->m_EditorStyle = Utils::GetEnumFromName<ImGuiLayer::Style>(styleNode.as<std::string>());
		if (auto node = data["EcoRendering"])
			bRenderOnlyWhenFocused = node.as<bool>();
		if (auto node = data["bUpdateAnimationsInEditor"])
			bUpdateAnimationsInEditor = node.as<bool>();
		if (auto node = data["DrawAxisGuizmo"])
			bDrawAxisGuizmo = node.as<bool>();
		if (auto node = data["DrawNavMesh"])
			bDrawNavMesh = node.as<bool>();
		if (auto node = data["DrawMeshAABBs"])
			bDrawMeshAABBs = node.as<bool>();
		if (auto node = data["StopSimulationKey"])
			stopSimulationKey = Utils::GetEnumFromName<Eagle::Key>(node.as<std::string>());
		if (auto VSyncNode = data["VSync"])
			bVSync = VSyncNode.as<bool>();
		if (auto node = data["GuizmoMode"])
			guizmoMode = Utils::GetEnumFromName<ImGuizmo::MODE>(node.as<std::string>());

		if (auto editorCameraNode = data["EditorCamera"])
		{
			camera.SetProjectionMode(Utils::GetEnumFromName<CameraProjectionMode>(editorCameraNode["ProjectionMode"].as<std::string>()));

			camera.SetPerspectiveVerticalFOV(editorCameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(editorCameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(editorCameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(editorCameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(editorCameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(editorCameraNode["OrthographicFarClip"].as<float>());
			if (auto node = editorCameraNode["ShadowFarClip"])
				camera.SetShadowFarClip(node.as<float>());
			if (auto node = editorCameraNode["DirLightShadowFarClip"])
				camera.SetDirLightShadowFarClip(node.as<float>());
			if (auto node = editorCameraNode["CascadesSplitAlpha"])
				camera.SetCascadesSplitAlpha(node.as<float>());
			if (auto node = editorCameraNode["CascadesSmoothTransitionAlpha"])
				camera.SetCascadesSmoothTransitionAlpha(node.as<float>());

			camera.SetMoveSpeed(editorCameraNode["MoveSpeed"].as<float>());
			camera.SetRotationSpeed(editorCameraNode["RotationSpeed"].as<float>());
		}
		
		Serializer::DeserializeRendererSettings(data, settings);

		editor->OnDeserialized(camera, windowSize, windowPos, settings, bWindowMaximized, bVSync, bRenderOnlyWhenFocused, bDrawNavMesh, bDrawMeshAABBs, bDrawAxisGuizmo, stopSimulationKey, bUpdateAnimationsInEditor, guizmoMode);
		return true;
	}
}
