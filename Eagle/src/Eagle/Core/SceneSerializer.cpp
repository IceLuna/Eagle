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
			const AssetHeader header = Utils::CreateAssetHeader(out, uint16_t(AssetType::Scene), 0, &totalSize);
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
		Ref<Asset> sceneAsset;
		const bool bAlwaysIncludeInGameBuilds = scene && AssetManager::Get(scene->GetGUID(), &sceneAsset) && sceneAsset->IsAlwaysIncludedInGameBuilds();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Scene);
		out << YAML::Key << "GUID" << YAML::Value << (scene ? scene->GetGUID() : GUID{});
		out << YAML::Key << "AlwaysIncludeInGameBuilds" << YAML::Value << bAlwaysIncludeInGameBuilds;

		if (!scene)
		{
			out << YAML::EndMap;
			return true;
		}

		out << YAML::Key << "Scene" << YAML::Value;
		out << YAML::BeginMap;

		//Editor camera
		const auto& camera = scene->EditorCamera;
		const auto& transform = camera.GetTransform();
		const auto& settings = scene->GetPhysicsSettings();

		// Only location & rotation are stored. Other settings are provided by the editor
		out << YAML::Key << "EditorCamera"	<< YAML::BeginMap;
		out << YAML::Key << "Location" << YAML::Value << transform.Location;
		out << YAML::Key << "Rotation" << YAML::Value << transform.Rotation;
		out << YAML::EndMap; //Editor Camera
		out << YAML::Key << "Gravity" << YAML::Value << settings.Gravity;
		out << YAML::Key << "PhysicsUpdateRate" << YAML::Value << settings.UpdateRate;
		out << YAML::Key << "PhysicsDebugOnPlay" << YAML::Value << settings.bDebugOnPlay;
		out << YAML::Key << "PhysicsDebugType" << YAML::Value << Utils::GetEnumName(settings.DebugType);

		Serializer::SerializeProjectCollisionGroupGUIDs(out);

		// Save active nav mesh components. It'll be used during deserialization to build the nav mesh after a scene has been loaded
		{
			const std::vector<NavigationMeshComponent*> navMeshes = scene->GetActiveNavMeshComponents();
			if (!navMeshes.empty())
			{
				out << YAML::Key << "NavMeshes" << YAML::Value << YAML::BeginSeq;
				for (const NavigationMeshComponent* navMesh : navMeshes)
				{
					out << YAML::BeginMap;
					out << YAML::Key << "Entity" << YAML::Value << navMesh->Parent.GetGUID();
					out << YAML::Key << "AgentType" << YAML::Value << navMesh->GetAgentTypeID();
					out << YAML::Key << "Built" << YAML::Value << (navMesh->GetNavMesh() != nullptr);
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
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

	bool SceneSerializer::Deserialize(const Ref<Scene>& scene, YAML::Node& baseNode, const std::function<bool(size_t done, size_t total)>& onProgress)
	{
		// The active nav mesh components that were saved
		struct SavedNavMesh
		{
			GUID Entity;
			uint32_t AgentTypeID = AINavigation::InvalidAgentTypeID;
			bool bBuilt = false;
		};

		AssetType assetType = AssetType::None;
		if (auto node = baseNode["Type"])
			assetType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (assetType != AssetType::Scene)
			return false;

		scene->SetGUID(baseNode["GUID"].as<GUID>());

		YAML::Node data = baseNode["Scene"];
		if (!data)
			return true; // It's an empty scene

		if (auto editorCameraNode = data["EditorCamera"])
		{
			auto& camera = scene->EditorCamera;

			Transform transform;
			transform.Location = editorCameraNode["Location"].as<glm::vec3>();
			transform.Rotation = editorCameraNode["Rotation"].as<Rotator>();
			
			camera.SetTransform(transform);
		}

		PhysicsSettings physics = scene->GetPhysicsSettings();
		if (auto node = data["Gravity"])
		{
			physics.Gravity = node.as<glm::vec3>();
		}
		if (auto node = data["PhysicsUpdateRate"])
		{
			physics.UpdateRate = node.as<uint32_t>();
		}
		if (auto node = data["PhysicsDebugOnPlay"])
		{
			physics.bDebugOnPlay = node.as<bool>();
		}
		if (auto node = data["PhysicsDebugType"])
		{
			physics.DebugType = Utils::GetEnumFromName<DebugType>(node.as<std::string>());
		}
		scene->SetPhysicsSettings(physics);

		const uint32_t collisionGroupValidMasks = Serializer::DeserializeProjectCollisionGroupGUIDs(data);

		std::vector<SavedNavMesh> navMeshes;
		if (auto node = data["NavMeshes"])
		{
			for (const auto& navMeshNode : node)
			{
				SavedNavMesh& navMesh = navMeshes.emplace_back();
				navMesh.Entity = navMeshNode["Entity"].as<GUID>();
				navMesh.AgentTypeID = navMeshNode["AgentType"].as<uint32_t>();
				navMesh.bBuilt = navMeshNode["Built"].as<bool>();
			}
		}

		DeserializeSkybox(scene, data);

		if (auto entities = data["Entities"])
		{
			//uint32_t - Entity's ID in the asset; Real entity ID; 
			std::unordered_map<uint32_t, Entity> allEntities;

			//uint32_t - entity that has an parent, uint32_t - parent id
			std::unordered_map<uint32_t, uint32_t> childs;

			const size_t entitiesCount = entities.size();
			size_t deserializedCount = 0;
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

				++deserializedCount;
				if (onProgress && !onProgress(deserializedCount, entitiesCount))
					return false;
			}

			for (const auto& element : childs)
			{
				Entity& parent = allEntities[element.second];
				Entity child((entt::entity)element.first, scene.get());
				child.SetParent(parent);
			}
		}

		// While the entities were loaded, the first component of each agent type became active. The saved ones become active instead.
		// Nothing is built while the entities are loaded, so no nav mesh is destroyed by it
		auto FindNavMeshComponent = [&scene](const GUID& guid) -> NavigationMeshComponent*
		{
			Entity entity = scene->GetEntityByGUID(guid);
			return entity && entity.HasComponent<NavigationMeshComponent>() ? &entity.GetComponent<NavigationMeshComponent>() : nullptr;
		};
		for (const SavedNavMesh& navMesh : navMeshes)
		{
			// A component whose type was removed from the project fell back to another type while it was loaded.
			// It doesn't take that type from the component that was saved as active for it
			NavigationMeshComponent* component = FindNavMeshComponent(navMesh.Entity);
			const bool bSameType = component && (component->GetAgentTypeID() == navMesh.AgentTypeID);
			if (bSameType)
				scene->SetActiveNavMeshComponent(*component);
		}

		// After all of them are active, so that building one doesn't depend on the order
		for (const SavedNavMesh& navMesh : navMeshes)
		{
			NavigationMeshComponent* component = navMesh.bBuilt ? FindNavMeshComponent(navMesh.Entity) : nullptr;
			if (component && component->IsActive())
				scene->BuildNavMesh(*component);
		}

		return true;
	}

	bool SceneSerializer::SerializeWithYaml(const Path& filepath, const std::string& yaml)
	{
		size_t totalSize = sizeof(AssetHeader);
		const AssetHeader header = Utils::CreateAssetHeader(yaml, uint16_t(AssetType::Scene), 0, &totalSize);
		ScopedDataBuffer buffer(totalSize);

		size_t offset = 0;
		Utils::WriteToBuffer(buffer, &header, sizeof(header), &offset);
		Utils::WriteStringToBuffer(buffer, yaml.c_str(), yaml.size(), &offset);

		return FileSystem::Write(filepath, buffer);
	}

	void SceneSerializer::SerializeSkybox(const Ref<Scene>& scene, YAML::Emitter& out)
	{
		const auto& settings = scene->GetSkyboxSettings();

		out << YAML::Key << "Skybox" << YAML::BeginMap;
		{
			if (settings.IBL)
				out << YAML::Key << "IBL" << YAML::Value << settings.IBL->GetGUID();
			out << YAML::Key << "Intensity" << YAML::Value << settings.Intensity;

			{
				const auto& sky = settings.Sky;
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
		out << YAML::Key << "bUseSky" << YAML::Value << settings.bUseSkyAsBackground;
		out << YAML::Key << "bRenderSkybox" << YAML::Value << settings.bRenderSkybox;
		out << YAML::Key << "bEnabled" << YAML::Value << settings.bEnabled;
		out << YAML::EndMap;
	}

	void SceneSerializer::DeserializeSkybox(const Ref<Scene>& scene, YAML::Node& node)
	{
		auto skyboxNode = node["Skybox"];
		if (!skyboxNode)
			return;

		SceneSkyboxSettings settings;
		if (auto iblNode = skyboxNode["IBL"])
			settings.IBL = GetAsset<AssetTextureCube>(iblNode);

		if (auto intensityNode = skyboxNode["Intensity"])
			settings.Intensity = intensityNode.as<float>();

		if (auto skyNode = skyboxNode["Sky"])
		{
			auto& sky = settings.Sky;
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

		if (auto node = skyboxNode["bUseSky"])
			settings.bUseSkyAsBackground = node.as<bool>();
		if (auto node = skyboxNode["bRenderSkybox"])
			settings.bRenderSkybox = node.as<bool>();
		if (auto node = skyboxNode["bEnabled"])
			settings.bEnabled = node.as<bool>();

		scene->SetSkyboxSettings(settings);
	}
}
