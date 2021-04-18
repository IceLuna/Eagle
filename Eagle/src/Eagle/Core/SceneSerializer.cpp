#include "egpch.h"

#include "SceneSerializer.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"

namespace YAML
{
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
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene)
	{
	}

	bool SceneSerializer::Serialize(const std::string& filepath)
	{
		EG_CORE_TRACE("Saving scene '{0}'", filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene"	<< YAML::Value << "Untitled";

		//Editor camera
		const auto& transform = m_Scene->m_EditorCamera.GetTransform();
		const auto& camera = m_Scene->m_EditorCamera;
		
		out << YAML::Key << "EditorCamera"	<< YAML::BeginMap;
		out << YAML::Key << "ProjectionMode" << YAML::Value << (int)camera.GetProjectionMode();
		out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
		out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
		out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
		out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
		out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
		out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
		out << YAML::Key << "MoveSpeed" << YAML::Value << camera.GetMoveSpeed();
		out << YAML::Key << "RotationSpeed" << YAML::Value << camera.GetRotationSpeed();
		out << YAML::Key << "Translation" << YAML::Value << transform.Translation;
		out << YAML::Key << "Rotation" << YAML::Value << transform.Rotation;
		out << YAML::EndMap; //Editor Camera


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

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load scene {0}. File doesn't exist!", filepath);
			return false;
		}
		
		YAML::Node data = YAML::LoadFile(filepath);
		if (!data["Scene"])
		{
			EG_CORE_WARN("Can't load scene {0}. File has invalid format!", filepath);
			return false;
		}
		EG_CORE_TRACE("Loading scene '{0}'", filepath);

		auto editorCameraNode = data["EditorCamera"];
		if (editorCameraNode)
		{
			auto& camera = m_Scene->m_EditorCamera;

			camera.SetProjectionMode((CameraProjectionMode)editorCameraNode["ProjectionMode"].as<int>());

			camera.SetPerspectiveVerticalFOV(editorCameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(editorCameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(editorCameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(editorCameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(editorCameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(editorCameraNode["OrthographicFarClip"].as<float>());

			camera.SetMoveSpeed(editorCameraNode["MoveSpeed"].as<float>());
			camera.SetRotationSpeed(editorCameraNode["RotationSpeed"].as<float>());

			Transform transform;
			transform.Translation = editorCameraNode["Translation"].as<glm::vec3>();
			transform.Rotation = editorCameraNode["Rotation"].as<glm::vec3>();
			
			camera.SetTransform(transform);
		}

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto& entityNode : entities)
			{
				DeserializeEntity(m_Scene, entityNode);
			}

			for (std::pair<uint32_t, uint32_t> element : m_Childs)
			{
				Entity& owner = m_AllEntities[element.second];
				Entity child((entt::entity)element.first, m_Scene.get());
				child.SetOwner(owner);
			}
		}

		return true;
	}

	bool SceneSerializer::SerializeBinary(const std::string& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	bool SceneSerializer::DeserializeBinary(const std::string& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity& entity)
	{
		uint32_t entityID = entity.GetID();

		out << YAML::BeginMap; //Entity

		out << YAML::Key << "EntityID" << YAML::Value << entityID; //TODO: Add entity ID

		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			auto& sceneNameComponent = entity.GetComponent<EntitySceneNameComponent>();
			auto& name = sceneNameComponent.Name;

			int ownerID = -1;
			if (Entity& owner = entity.GetOwner())
			{
				ownerID = (int)owner.GetID();
			}

			out << YAML::Key << "EntitySceneParams";
			out << YAML::BeginMap; //EntitySceneName

			out << YAML::Key << "Name"  << YAML::Value << name;
			out << YAML::Key << "Owner" << YAML::Value << ownerID;

			out << YAML::EndMap; //EntitySceneParams
		}

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& worldTransform = entity.GetWorldTransform();
			const auto& relativeTransform = entity.GetRelativeTransform();

			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; //TransformComponent

			out << YAML::Key << "WorldTranslation"		<< YAML::Value << worldTransform.Translation;
			out << YAML::Key << "WorldRotation"			<< YAML::Value << worldTransform.Rotation;
			out << YAML::Key << "WorldScale"			<< YAML::Value << worldTransform.Scale3D;

			out << YAML::EndMap; //TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			const auto& relativeTransform = cameraComponent.GetRelativeTransform();
			auto& camera = entity.GetComponent<CameraComponent>().Camera;

			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; //CameraComponent;

			out << YAML::Key << "Camera";
			out << YAML::BeginMap; //Camera

			out << YAML::Key << "ProjectionMode"			<< YAML::Value << (int)camera.GetProjectionMode();
			out << YAML::Key << "PerspectiveVerticalFOV"	<< YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip"		<< YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip"		<< YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize"			<< YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip"		<< YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip"		<< YAML::Value << camera.GetOrthographicFarClip();

			out << YAML::EndMap; //Camera

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "Primary"			<< YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio"	<< YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			auto& spriteComponent = entity.GetComponent<SpriteComponent>();
			auto& material = spriteComponent.Material;
			const auto& relativeTransform = spriteComponent.GetRelativeTransform();

			out << YAML::Key << "SpriteComponent";
			out << YAML::BeginMap; //SpriteComponent

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "Material";
			out << YAML::BeginMap; //Material
			out << YAML::Key << "DiffuseTexture" << YAML::Value << material.DiffuseTexture->GetPath();
			out << YAML::Key << "SpecularTexture" << YAML::Value << material.SpecularTexture->GetPath();
			out << YAML::Key << "Shininess" << YAML::Value << material.Shininess;
			out << YAML::EndMap; //Material

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();
			const auto& relativeTransform = pointLightComponent.GetRelativeTransform();

			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "LightColor" << YAML::Value << pointLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << pointLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << pointLightComponent.Specular;
			out << YAML::Key << "Distance" << YAML::Value << pointLightComponent.Distance;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();
			const auto& relativeTransform = directionalLightComponent.GetRelativeTransform();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << directionalLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << directionalLightComponent.Specular;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();
			const auto& relativeTransform = spotLightComponent.GetRelativeTransform();

			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "LightColor" << YAML::Value << spotLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << spotLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << spotLightComponent.Specular;
			out << YAML::Key << "InnerCutOffAngle" << YAML::Value << spotLightComponent.InnerCutOffAngle;
			out << YAML::Key << "OuterCutOffAngle" << YAML::Value << spotLightComponent.OuterCutOffAngle;

			out << YAML::EndMap; //SpriteComponent
		}

		out << YAML::EndMap; //Entity
	}

	void SceneSerializer::DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode)
	{
		//TODO: Add tags serialization and deserialization
		const uint32_t id = entityNode["EntityID"].as<uint32_t>();

		std::string name;
		int ownerID = -1;
		auto sceneNameComponentNode = entityNode["EntitySceneParams"];
		if (sceneNameComponentNode)
		{
			name = sceneNameComponentNode["Name"].as<std::string>();
			ownerID = sceneNameComponentNode["Owner"].as<int>();
		}

		Entity deserializedEntity = scene->CreateEntity(name);
		m_AllEntities[id] = deserializedEntity;

		if (ownerID != -1)
		{
			m_Childs[deserializedEntity.GetID()] = ownerID;
		}

		auto transformComponentNode = entityNode["TransformComponent"];
		if (transformComponentNode)
		{
			//Every entity has a transform component
			Transform worldTransform;

			worldTransform.Translation = transformComponentNode["WorldTranslation"].as<glm::vec3>();
			worldTransform.Rotation = transformComponentNode["WorldRotation"].as<glm::vec3>();
			worldTransform.Scale3D = transformComponentNode["WorldScale"].as<glm::vec3>();

			deserializedEntity.SetWorldTransform(worldTransform);
		}

		auto cameraComponentNode = entityNode["CameraComponent"];
		if (cameraComponentNode)
		{
			auto& cameraComponent = deserializedEntity.AddComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;
			Transform relativeTransform;

			auto& cameraNode = cameraComponentNode["Camera"];
			camera.SetProjectionMode((CameraProjectionMode)cameraNode["ProjectionMode"].as<int>());
			
			camera.SetPerspectiveVerticalFOV(cameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(cameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(cameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(cameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(cameraNode["OrthographicFarClip"].as<float>());

			relativeTransform.Translation = cameraComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = cameraComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = cameraComponentNode["RelativeScale"].as<glm::vec3>();

			cameraComponent.SetRelativeTransform(relativeTransform);

			cameraComponent.Primary = cameraComponentNode["Primary"].as<bool>();
			cameraComponent.FixedAspectRatio = cameraComponentNode["FixedAspectRatio"].as<bool>();
		}

		auto spriteComponentNode = entityNode["SpriteComponent"];
		if (spriteComponentNode)
		{
			auto& spriteComponent = deserializedEntity.AddComponent<SpriteComponent>();
			auto& material = spriteComponent.Material;
			Transform relativeTransform;

			relativeTransform.Translation = spriteComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = spriteComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = spriteComponentNode["RelativeScale"].as<glm::vec3>();
			
			auto& materialNode = spriteComponentNode["Material"];
			if (materialNode)
			{
				if (materialNode["DiffuseTexture"])
				{
					const std::string& path = materialNode["DiffuseTexture"].as<std::string>();
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture))
					{
						material.DiffuseTexture = texture;
					}
					else
					{
						material.DiffuseTexture = Texture2D::Create(path);
					}
				}

				if (materialNode["SpecularTexture"])
				{
					const std::string& path = materialNode["SpecularTexture"].as<std::string>();
					Ref<Texture> texture;
					if (TextureLibrary::Get(path, &texture))
					{
						material.SpecularTexture = texture;
					}
					else
					{
						material.SpecularTexture = Texture2D::Create(path);
					}
				}

				material.Shininess = materialNode["Shininess"].as<float>();
			}

			spriteComponent.SetRelativeTransform(relativeTransform);
		}

		auto pointLightComponentNode = entityNode["PointLightComponent"];
		if (pointLightComponentNode)
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();
			Transform relativeTransform;

			relativeTransform.Translation = pointLightComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = pointLightComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = pointLightComponentNode["RelativeScale"].as<glm::vec3>();

			pointLightComponent.LightColor = pointLightComponentNode["LightColor"].as<glm::vec4>();
			pointLightComponent.Ambient = pointLightComponentNode["Ambient"].as<glm::vec3>();
			pointLightComponent.Specular = pointLightComponentNode["Specular"].as<glm::vec3>();
			if (pointLightComponentNode["Distance"])
				pointLightComponent.Distance = pointLightComponentNode["Distance"].as<float>();

			pointLightComponent.SetRelativeTransform(relativeTransform);
		}

		auto directionalLightComponentNode = entityNode["DirectionalLightComponent"];
		if (directionalLightComponentNode)
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();
			Transform relativeTransform;

			relativeTransform.Translation = directionalLightComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = directionalLightComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = directionalLightComponentNode["RelativeScale"].as<glm::vec3>();

			directionalLightComponent.LightColor = directionalLightComponentNode["LightColor"].as<glm::vec4>();
			directionalLightComponent.Ambient = directionalLightComponentNode["Ambient"].as<glm::vec3>();
			directionalLightComponent.Specular = directionalLightComponentNode["Specular"].as<glm::vec3>();

			directionalLightComponent.SetRelativeTransform(relativeTransform);
		}

		auto spotLightComponentNode = entityNode["SpotLightComponent"];
		if (spotLightComponentNode)
		{
			auto& spotLightComponent = deserializedEntity.AddComponent<SpotLightComponent>();
			Transform relativeTransform;

			relativeTransform.Translation = spotLightComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = spotLightComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = spotLightComponentNode["RelativeScale"].as<glm::vec3>();

			spotLightComponent.LightColor = spotLightComponentNode["LightColor"].as<glm::vec4>();
			spotLightComponent.Ambient = spotLightComponentNode["Ambient"].as<glm::vec3>();
			spotLightComponent.Specular = spotLightComponentNode["Specular"].as<glm::vec3>();

			if (spotLightComponentNode["InnerCutOffAngle"])
			{
				spotLightComponent.InnerCutOffAngle = spotLightComponentNode["InnerCutOffAngle"].as<float>();
				spotLightComponent.OuterCutOffAngle = spotLightComponentNode["OuterCutOffAngle"].as<float>();
			}

			spotLightComponent.SetRelativeTransform(relativeTransform);
		}
	}
}
