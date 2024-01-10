#include "egpch.h"

#include "SceneSerializer.h"
#include "Serializer.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"

namespace Eagle
{
	template<typename AssetType>
	static Ref<AssetType> GetAsset(const YAML::Node& node)
	{
		Ref<AssetType> result;
		if (node)
		{
			Ref<Asset> asset;
			if (AssetManager::Get(node.as<GUID>(), &asset))
				result = Cast<AssetType>(asset);
		}
		return result;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene)
	{}

	bool SceneSerializer::Serialize(const Path& filepath)
	{
		EG_CORE_TRACE("Saving Scene at '{0}'", std::filesystem::absolute(filepath));

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Scene);
		out << YAML::Key << "GUID" << YAML::Value << m_Scene->GetGUID();

		//Editor camera
		const auto& transform = m_Scene->m_EditorCamera.GetTransform();
		const auto& camera = m_Scene->m_EditorCamera;
		
		out << YAML::Key << "EditorCamera"	<< YAML::BeginMap;
		out << YAML::Key << "ProjectionMode" << YAML::Value << Utils::GetEnumName(camera.GetProjectionMode());
		out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
		out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
		out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
		out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
		out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
		out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
		out << YAML::Key << "ShadowFarClip" << YAML::Value << camera.GetShadowFarClip();
		out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();
		out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
		out << YAML::Key << "MoveSpeed" << YAML::Value << camera.GetMoveSpeed();
		out << YAML::Key << "RotationSpeed" << YAML::Value << camera.GetRotationSpeed();
		out << YAML::Key << "Location" << YAML::Value << transform.Location;
		out << YAML::Key << "Rotation" << YAML::Value << transform.Rotation;
		out << YAML::EndMap; //Editor Camera

		SerializeSkybox(out);

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		std::vector<Entity> entities;
		entities.reserve(m_Scene->m_Registry.alive());

		m_Scene->m_Registry.each([&](auto entityID)
		{
			entities.emplace_back(entityID, m_Scene.get());
		});

		for (auto it = entities.rbegin(); it != entities.rend(); ++it)
		{
			SerializeEntity(out, *it);
		}

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();

		return true;
	}

	bool SceneSerializer::Deserialize(const Path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load scene {0}. File doesn't exist!", std::filesystem::absolute(filepath));
			return false;
		}
		
		YAML::Node data = YAML::LoadFile(filepath.string());
		AssetType assetType = AssetType::None;
		if (auto node = data["Type"])
			assetType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (assetType != AssetType::Scene)
		{
			EG_CORE_WARN("Can't load scene {0}. Invalid asset!", std::filesystem::absolute(filepath));
			return false;
		}

		EG_CORE_TRACE("Loading scene '{0}'", std::filesystem::absolute(filepath));

		m_Scene->SetGUID(data["GUID"].as<GUID>());

		if (auto editorCameraNode = data["EditorCamera"])
		{
			auto& camera = m_Scene->m_EditorCamera;

			camera.SetProjectionMode(Utils::GetEnumFromName<CameraProjectionMode>(editorCameraNode["ProjectionMode"].as<std::string>()));

			camera.SetPerspectiveVerticalFOV(editorCameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(editorCameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(editorCameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(editorCameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(editorCameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(editorCameraNode["OrthographicFarClip"].as<float>());
			if (auto node = editorCameraNode["ShadowFarClip"])
				camera.SetShadowFarClip(node.as<float>());
			if (auto node = editorCameraNode["CascadesSplitAlpha"])
				camera.SetCascadesSplitAlpha(node.as<float>());
			if (auto node = editorCameraNode["CascadesSmoothTransitionAlpha"])
				camera.SetCascadesSmoothTransitionAlpha(node.as<float>());

			camera.SetMoveSpeed(editorCameraNode["MoveSpeed"].as<float>());
			camera.SetRotationSpeed(editorCameraNode["RotationSpeed"].as<float>());

			Transform transform;
			transform.Location = editorCameraNode["Location"].as<glm::vec3>();
			transform.Rotation = editorCameraNode["Rotation"].as<Rotator>();
			
			camera.SetTransform(transform);
		}

		DeserializeSkybox(data);

		if (auto entities = data["Entities"])
		{
			for (auto& entityNode : entities)
				DeserializeEntity(m_Scene, entityNode);

			for (std::pair<uint32_t, uint32_t> element : m_Childs)
			{
				Entity& parent = m_AllEntities[element.second];
				Entity child((entt::entity)element.first, m_Scene.get());
				child.SetParent(parent);
			}
		}

		EG_CORE_TRACE("Loaded scene '{0}'", std::filesystem::absolute(filepath));
		return true;
	}

	bool SceneSerializer::SerializeBinary(const Path& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	bool SceneSerializer::DeserializeBinary(const Path& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity& entity)
	{
		uint32_t entityID = entity.GetID();

		out << YAML::BeginMap; //Entity

		out << YAML::Key << "EntityID" << YAML::Value << entityID;
		Serializer::SerializeEntity(out, entity);
		
		out << YAML::EndMap; //Entity
	}

	void SceneSerializer::SerializeSkybox(YAML::Emitter& out)
	{
		const auto& sceneRenderer = m_Scene->GetSceneRenderer();
		out << YAML::Key << "Skybox" << YAML::BeginMap;
		{
			if (const Ref<AssetTextureCube>& ibl = sceneRenderer->GetSkybox())
				out << YAML::Key << "IBL" << YAML::Value << ibl->GetGUID();
			out << YAML::Key << "Intensity" << YAML::Value << sceneRenderer->GetSkyboxIntensity();

			{
				const auto& sky = sceneRenderer->GetSkySettings();
				out << YAML::Key << "Sky" << YAML::BeginMap;
				out << YAML::Key << "SunPos" << YAML::Value << sky.SunPos;
				out << YAML::Key << "SkyIntensity" << YAML::Value << sky.SkyIntensity;
				out << YAML::Key << "CloudsIntensity" << YAML::Value << sky.CloudsIntensity;
				out << YAML::Key << "CloudsColor" << YAML::Value << sky.CloudsColor;
				out << YAML::Key << "Scattering" << YAML::Value << sky.Scattering;
				out << YAML::Key << "Cirrus" << YAML::Value << sky.Cirrus;
				out << YAML::Key << "Cumulus" << YAML::Value << sky.Cumulus;
				out << YAML::Key << "CumulusLayers" << YAML::Value << sky.CumulusLayers;
				out << YAML::Key << "bEnableCirrusClouds" << YAML::Value << sky.bEnableCirrusClouds;
				out << YAML::Key << "bEnableCumulusClouds" << YAML::Value << sky.bEnableCumulusClouds;
				out << YAML::EndMap;
			}
		}
		out << YAML::Key << "bUseSky" << YAML::Value << sceneRenderer->GetUseSkyAsBackground();
		out << YAML::Key << "bEnabled" << YAML::Value << sceneRenderer->IsSkyboxEnabled();
		out << YAML::EndMap;
	}

	void SceneSerializer::DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode)
	{
		//TODO: Add tags serialization and deserialization
		const uint32_t id = entityNode["EntityID"].as<uint32_t>();

		std::string name;
		int parentID = -1;
		if (auto sceneNameComponentNode = entityNode["EntitySceneParams"])
		{
			name = sceneNameComponentNode["Name"].as<std::string>();
			parentID = sceneNameComponentNode["Parent"].as<int>();
		}

		Entity deserializedEntity = scene->CreateEntity(name);
		m_AllEntities[id] = deserializedEntity;

		if (parentID != -1)
		{
			m_Childs[deserializedEntity.GetID()] = parentID;
		}

		Serializer::DeserializeEntity(deserializedEntity, entityNode);
	}

	void SceneSerializer::DeserializeSkybox(YAML::Node& node)
	{
		auto skyboxNode = node["Skybox"];
		if (!skyboxNode)
			return;

		auto& sceneRenderer = m_Scene->GetSceneRenderer();
		Ref<AssetTextureCube> skybox;
		float skyboxIntensity = 1.f;

		if (auto iblNode = skyboxNode["IBL"])
			skybox = GetAsset<AssetTextureCube>(iblNode);

		if (auto intensityNode = skyboxNode["Intensity"])
			skyboxIntensity = intensityNode.as<float>();

		sceneRenderer->SetSkybox(skybox);
		sceneRenderer->SetSkyboxIntensity(skyboxIntensity);

		SkySettings sky{};
		if (auto skyNode = skyboxNode["Sky"])
		{
			sky.SunPos = skyNode["SunPos"].as<glm::vec3>();
			sky.SkyIntensity = skyNode["SkyIntensity"].as<float>();
			sky.CloudsIntensity = skyNode["CloudsIntensity"].as<float>();
			sky.CloudsColor = skyNode["CloudsColor"].as<glm::vec3>();
			sky.Scattering = skyNode["Scattering"].as<float>();
			sky.Cirrus = skyNode["Cirrus"].as<float>();
			sky.Cumulus = skyNode["Cumulus"].as<float>();
			sky.CumulusLayers = skyNode["CumulusLayers"].as<uint32_t>();
			sky.bEnableCirrusClouds = skyNode["bEnableCirrusClouds"].as<bool>();
			sky.bEnableCumulusClouds = skyNode["bEnableCumulusClouds"].as<bool>();
		}
		sceneRenderer->SetSkybox(sky);
		sceneRenderer->SetUseSkyAsBackground(skyboxNode["bUseSky"].as<bool>());

		bool bSkyboxEnabled = true;
		if (auto node = skyboxNode["bEnabled"])
			bSkyboxEnabled = node.as<bool>();
		sceneRenderer->SetSkyboxEnabled(bSkyboxEnabled);
	}
}
