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
		m_Scene->m_Registry.each([&](auto entityID)
		{
			Entity entity = {entityID, m_Scene.get()};

			if (!entity)
				return;
			
			SerializeEntity(out, entity);
		});

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

		if (entity.HasComponent<LightComponent>())
		{
			auto& lightComponent = entity.GetComponent<LightComponent>();
			const auto& relativeTransform = lightComponent.GetRelativeTransform();

			out << YAML::Key << "LightComponent";
			out << YAML::BeginMap; //SpriteComponent

			out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
			out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
			out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;

			out << YAML::Key << "LightColor" << YAML::Value << lightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << lightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << lightComponent.Specular;
			out << YAML::Key << "Distance" << YAML::Value << lightComponent.Distance;

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

		auto lightComponentNode = entityNode["LightComponent"];
		if (lightComponentNode)
		{
			auto& lightComponent = deserializedEntity.AddComponent<LightComponent>();
			Transform relativeTransform;

			relativeTransform.Translation = lightComponentNode["RelativeTranslation"].as<glm::vec3>();
			relativeTransform.Rotation = lightComponentNode["RelativeRotation"].as<glm::vec3>();
			relativeTransform.Scale3D = lightComponentNode["RelativeScale"].as<glm::vec3>();

			lightComponent.LightColor = lightComponentNode["LightColor"].as<glm::vec4>();
			lightComponent.Ambient = lightComponentNode["Ambient"].as<glm::vec3>();
			lightComponent.Specular = lightComponentNode["Specular"].as<glm::vec3>();
			if (lightComponentNode["Distance"])
				lightComponent.Distance = lightComponentNode["Distance"].as<float>();

			lightComponent.SetRelativeTransform(relativeTransform);
		}
	}
}
