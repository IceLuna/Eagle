#include "egpch.h"

#include "SceneSerializer.h"
#include "Serializer.h"

#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"
#include "Eagle/Core/Project.h"
#include "Eagle/Utils/SerializerUtils.h"

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

	bool SceneSerializer::Serialize(const Ref<Scene>& scene, const Path& filepath)
	{
		EG_CORE_TRACE("Saving Scene at '{0}'", std::filesystem::absolute(filepath));

		YAML::Emitter out;
		if (Serialize(scene, out))
		{
			size_t totalSize = sizeof(AssetHeader);
			const AssetHeader header = Utils::CreateHeader(out, &totalSize);
			ScopedDataBuffer buffer(totalSize);

			size_t offset = 0;
			Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
			Utils::WriteYaml(buffer, out, &offset);

			return FileSystem::Write(filepath, buffer);
		}

		return false;
	}

	bool SceneSerializer::Serialize(const Ref<Scene>& scene, YAML::Emitter& out)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Scene);
		out << YAML::Key << "GUID" << YAML::Value << (scene ? scene->GetGUID() : GUID{});

		if (!scene)
		{
			out << YAML::EndMap;
			return true;
		}

		out << YAML::Key << "Scene" << YAML::Value;
		out << YAML::BeginMap;

		//Editor camera
		const auto& camera = scene->GetEditorCamera();
		const auto& transform = camera.GetTransform();
		
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
		out << YAML::Key << "Gravity" << YAML::Value << scene->GetGravity();
		out << YAML::Key << "PhysicsUpdateRate" << YAML::Value << scene->GetPhysicsUpdateRate();
		out << YAML::Key << "PhysicsDebugOnPlay" << YAML::Value << scene->IsPhysicsDebugOnPlayEnabled();
		out << YAML::Key << "PhysicsDebugType" << YAML::Value << Utils::GetEnumName(scene->GetPhysicsDebugType());

		Serializer::SerializeProjectCollisionGroupGUIDs(out);

		// Save EntityID that has a valid nav mesh. It'll be used during deserialization to build the nav mesh after a scene has been loaded
		{
			GUID navMeshEntity = GUID(0, 0);
			if (scene->GetNavMesh())
			{
				auto view = scene->GetAllEntitiesWith<NavigationMeshComponent>();
				for (auto& e : view)
				{
					Entity entity(e, scene.get());
					const auto& component = entity.GetComponent<NavigationMeshComponent>();
					if (component.GetNavMesh())
					{
						navMeshEntity = entity.GetGUID();
						break;
					}
				}
			}
			if (!navMeshEntity.IsNull())
			{
				out << YAML::Key << "NavMesh" << YAML::Value << navMeshEntity;
			}
		}

		SerializeSkybox(scene, out);

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		scene->OnEach([&out](const Entity& entity)
		{
			Serializer::SerializeEntity(out, entity);
		});

		out << YAML::EndSeq;
		out << YAML::EndMap;
		out << YAML::EndMap;

		return true;
	}

	bool SceneSerializer::Deserialize(const Ref<Scene>& scene, const Path& filepath)
	{
		const ScopedDataBuffer data = FileSystem::Read(filepath);
		if (data.Size() == 0)
		{
			EG_CORE_ERROR("Failed to deserialize a scene: {}", filepath);
			return false;
		}

		return Deserialize(scene, data.GetDataBuffer());
	}

	bool SceneSerializer::Deserialize(const Ref<Scene>& scene, const DataBuffer& data)
	{
		if (data.Size == 0)
		{
			return false;
		}

		YAML::Node baseNode;
		Utils::ReadYAML(data, &baseNode);

		if (Deserialize(scene, baseNode) == false)
		{
			EG_CORE_ERROR("Can't load the scene. Invalid asset!");
			return false;
		}

		return true;
	}

	bool SceneSerializer::Deserialize(const Ref<Scene>& scene, YAML::Node& baseNode)
	{
		AssetType assetType = AssetType::None;
		if (auto node = baseNode["Type"])
			assetType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (assetType != AssetType::Scene)
			return false;

		scene->SetGUID(baseNode["GUID"].as<GUID>());

		YAML::Node data = baseNode["Scene"];
		if (!data)
			return false;

		GUID navMeshEntityGUID = GUID(0, 0);

		if (auto editorCameraNode = data["EditorCamera"])
		{
			auto& camera = scene->GetEditorCamera();

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
		if (auto node = data["Gravity"])
		{
			scene->SetGravity(node.as<glm::vec3>());
		}
		if (auto node = data["PhysicsUpdateRate"])
		{
			scene->SetPhysicsUpdateRate(node.as<uint32_t>());
		}
		if (auto node = data["PhysicsDebugOnPlay"])
		{
			scene->SetPhysicsDebugOnPlay(node.as<bool>());
		}
		if (auto node = data["PhysicsDebugType"])
		{
			scene->SetPhysicsDebugType(Utils::GetEnumFromName<DebugType>(node.as<std::string>()));
		}

		const uint32_t collisionGroupValidMasks = Serializer::DeserializeProjectCollisionGroupGUIDs(data);

		if (auto node = data["NavMesh"])
		{
			navMeshEntityGUID = node.as<GUID>();
		}

		DeserializeSkybox(scene, data);

		if (auto entities = data["Entities"])
		{
			//uint32_t - Entity's ID in the asset; Real entity ID; 
			std::unordered_map<uint32_t, Entity> allEntities;

			//uint32_t - entity that has an parent, uint32_t - parent id
			std::unordered_map<uint32_t, uint32_t> childs;

			for (const auto& entityNode : entities)
			{
				uint32_t id;
				int parentID = -1;
				Entity deserializedEntity = Serializer::DeserializeEntity(scene, entityNode, collisionGroupValidMasks, &id, &parentID);

				allEntities[id] = deserializedEntity;
				if (parentID != -1)
				{
					childs[deserializedEntity.GetID()] = parentID;
				}
			}

			for (const auto& element : childs)
			{
				Entity& parent = allEntities[element.second];
				Entity child((entt::entity)element.first, scene.get());
				child.SetParent(parent);
			}
		}

		if (!navMeshEntityGUID.IsNull())
		{
			Entity entity = scene->GetEntityByGUID(navMeshEntityGUID);
			EG_CORE_ASSERT(entity && entity.HasComponent<NavigationMeshComponent>());
			scene->BuildNavMesh(&entity.GetComponent<NavigationMeshComponent>());
		}

		return true;
	}

	bool SceneSerializer::SerializeWithYaml(const Path& filepath, const std::string& yaml)
	{
		size_t totalSize = sizeof(AssetHeader);
		const AssetHeader header = Utils::CreateHeader(yaml, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteStringToBuffer(buffer, yaml.c_str(), yaml.size(), &offset);

		return FileSystem::Write(filepath, buffer);
	}

	void SceneSerializer::SerializeSkybox(const Ref<Scene>& scene, YAML::Emitter& out)
	{
		out << YAML::Key << "Skybox" << YAML::BeginMap;
		{
			if (const Ref<AssetTextureCube>& ibl = scene->GetSkybox())
				out << YAML::Key << "IBL" << YAML::Value << ibl->GetGUID();
			out << YAML::Key << "Intensity" << YAML::Value << scene->GetSkyboxIntensity();

			{
				const auto& sky = scene->GetSkySettings();
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
		out << YAML::Key << "bUseSky" << YAML::Value << scene->GetUseSkyAsBackground();
		out << YAML::Key << "bRenderSkybox" << YAML::Value << scene->IsRenderSkyboxEnabled();
		out << YAML::Key << "bEnabled" << YAML::Value << scene->IsSkyboxEnabled();
		out << YAML::EndMap;
	}

	void SceneSerializer::DeserializeSkybox(const Ref<Scene>& scene, YAML::Node& node)
	{
		auto skyboxNode = node["Skybox"];
		if (!skyboxNode)
			return;

		Ref<AssetTextureCube> skybox;
		float skyboxIntensity = 1.f;

		if (auto iblNode = skyboxNode["IBL"])
			skybox = GetAsset<AssetTextureCube>(iblNode);

		if (auto intensityNode = skyboxNode["Intensity"])
			skyboxIntensity = intensityNode.as<float>();

		scene->SetSkybox(skybox);
		scene->SetSkyboxIntensity(skyboxIntensity);

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
		scene->SetSkybox(sky);
		scene->SetUseSkyAsBackground(skyboxNode["bUseSky"].as<bool>());

		bool bRenderSkybox = true;
		if (auto node = skyboxNode["bRenderSkybox"])
			bRenderSkybox = node.as<bool>();
		scene->SetRenderSkybox(bRenderSkybox);

		bool bSkyboxEnabled = true;
		if (auto node = skyboxNode["bEnabled"])
			bSkyboxEnabled = node.as<bool>();
		scene->SetSkyboxEnabled(bSkyboxEnabled);
	}
}
