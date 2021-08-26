#include "egpch.h"

#include "SceneSerializer.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Renderer/Shader.h"

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

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		EG_CORE_TRACE("Saving Scene at '{0}'", filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene"	<< YAML::Value << "Untitled";
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Gamma" << YAML::Value << m_Scene->GetSceneGamma();
		out << YAML::Key << "Exposure" << YAML::Value << m_Scene->GetSceneExposure();

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

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load scene {0}. File doesn't exist!", filepath);
			return false;
		}
		
		YAML::Node data = YAML::LoadFile(filepath.string());
		if (!data["Scene"])
		{
			EG_CORE_WARN("Can't load scene {0}. File has invalid format!", filepath);
			return false;
		}
		EG_CORE_TRACE("Loading scene '{0}'", filepath);

		if (auto gammaNode = data["Gamma"])
			m_Scene->SetSceneGamma(data["Gamma"].as<float>());
		if (auto exposureNode = data["Exposure"])
			m_Scene->SetSceneExposure(data["Exposure"].as<float>());

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

		DeserializeSkybox(data);

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

	bool SceneSerializer::SerializeBinary(const std::filesystem::path& filepath)
	{
		EG_CORE_ASSERT(false, "Not supported yet");
		return false;
	}

	bool SceneSerializer::DeserializeBinary(const std::filesystem::path& filepath)
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

			SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary"			<< YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio"	<< YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			auto& spriteComponent = entity.GetComponent<SpriteComponent>();
			auto& material = spriteComponent.Material;

			out << YAML::Key << "SpriteComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spriteComponent.GetRelativeTransform());

			out << YAML::Key << "bSubTexture" << YAML::Value << spriteComponent.bSubTexture;
			out << YAML::Key << "SubTextureCoords" << YAML::Value << spriteComponent.SubTextureCoords;
			out << YAML::Key << "SpriteSize" << YAML::Value << spriteComponent.SpriteSize;
			out << YAML::Key << "SpriteSizeCoef" << YAML::Value << spriteComponent.SpriteSizeCoef;

			SerializeMaterial(out, material);

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			auto& smComponent = entity.GetComponent<StaticMeshComponent>();
			auto& sm = smComponent.StaticMesh;
			auto& material = sm->Material;

			std::filesystem::path currentPath = std::filesystem::current_path();
			std::filesystem::path smRelPath = std::filesystem::relative(sm->GetPath(), currentPath);

			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; //StaticMeshComponent

			out << YAML::Key << "Path" << YAML::Value << smRelPath.string();
			out << YAML::Key << "Index" << YAML::Value << sm->GetIndex();
			out << YAML::Key << "MadeOfMultipleMeshes" << YAML::Value << sm->MadeOfMultipleMeshes();

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());
			SerializeMaterial(out, material);

			out << YAML::EndMap; //StaticMeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, pointLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << pointLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << pointLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << pointLightComponent.Specular;
			out << YAML::Key << "Distance" << YAML::Value << pointLightComponent.Distance;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, directionalLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << directionalLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << directionalLightComponent.Specular;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();

			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spotLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << spotLightComponent.LightColor;
			out << YAML::Key << "Ambient" << YAML::Value << spotLightComponent.Ambient;
			out << YAML::Key << "Specular" << YAML::Value << spotLightComponent.Specular;
			out << YAML::Key << "InnerCutOffAngle" << YAML::Value << spotLightComponent.InnerCutOffAngle;
			out << YAML::Key << "OuterCutOffAngle" << YAML::Value << spotLightComponent.OuterCutOffAngle;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();

			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap;

			out << YAML::Key << "ModuleName" << YAML::Value << scriptComponent.ModuleName;

			out << YAML::EndMap;
		}

		out << YAML::EndMap; //Entity
	}

	void SceneSerializer::SerializeSkybox(YAML::Emitter& out)
	{
		//Skybox
		if (m_Scene->m_Cubemap)
		{
			constexpr char* sides[] = { "Right", "Left", "Top", "Bottom", "Front", "Back" };
			const auto& skyboxTextures = m_Scene->m_Cubemap->GetTextures();
			std::filesystem::path currentPath = std::filesystem::current_path();

			out << YAML::Key << "Skybox" << YAML::BeginMap;
			for (int i = 0; i < skyboxTextures.size(); ++i)
			{
				std::filesystem::path texturePath = std::filesystem::relative(skyboxTextures[i]->GetPath(), currentPath);
				if (texturePath.empty())
					texturePath = skyboxTextures[i]->GetPath();

				out << YAML::Key << sides[i];
				out << YAML::BeginMap;
				out << YAML::Key << "Path" << YAML::Value << texturePath.string();
				out << YAML::Key << "sRGB" << YAML::Value << skyboxTextures[i]->IsSRGB();
				out << YAML::EndMap;
			}
			out << YAML::Key << "Enabled" << YAML::Value << m_Scene->bEnableSkybox;

			out << YAML::EndMap; //Skybox
		}
	}

	void SceneSerializer::SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform)
	{
		out << YAML::Key << "RelativeTranslation" << YAML::Value << relativeTransform.Translation;
		out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
		out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;
	}

	void SceneSerializer::SerializeMaterial(YAML::Emitter& out, const Ref<Material>& material)
	{
		std::filesystem::path currentPath = std::filesystem::current_path();
		std::filesystem::path shaderRelPath = std::filesystem::relative(material->Shader->GetPath(), currentPath);

		out << YAML::Key << "Material";
		out << YAML::BeginMap; //Material

		SerializeTexture(out, material->DiffuseTexture, "DiffuseTexture");
		SerializeTexture(out, material->SpecularTexture, "SpecularTexture");
		SerializeTexture(out, material->NormalTexture, "NormalTexture");

		out << YAML::Key << "Shader" << YAML::Value << shaderRelPath.string();
		out << YAML::Key << "TintColor" << YAML::Value << material->TintColor;
		out << YAML::Key << "TilingFactor" << YAML::Value << material->TilingFactor;
		out << YAML::Key << "Shininess" << YAML::Value << material->Shininess;
		out << YAML::EndMap; //Material
	}

	void SceneSerializer::SerializeTexture(YAML::Emitter& out, const Ref<Texture>& texture, const std::string& textureName)
	{
		bool bValidTexture = texture.operator bool();
		if (bValidTexture)
		{
			std::filesystem::path currentPath = std::filesystem::current_path();
			std::filesystem::path textureRelPath = std::filesystem::relative(texture->GetPath(), currentPath);
			if (textureRelPath.empty())
				textureRelPath = texture->GetPath();

			out << YAML::Key << textureName;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << textureRelPath.string();
			out << YAML::Key << "sRGB" << YAML::Value << texture->IsSRGB();
			out << YAML::EndMap;
		}
		else
		{
			out << YAML::Key << textureName;
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << "None";
			out << YAML::EndMap;
		}
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

			DeserializeRelativeTransform(cameraComponentNode, relativeTransform);

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

			DeserializeRelativeTransform(spriteComponentNode, relativeTransform);
			if (auto materialNode = spriteComponentNode["Material"])
				DeserializeMaterial(materialNode, material);

			auto subtextureNode = spriteComponentNode["bSubTexture"];
			if (subtextureNode)
			{
				spriteComponent.bSubTexture = spriteComponentNode["bSubTexture"].as<bool>();
				spriteComponent.SubTextureCoords = spriteComponentNode["SubTextureCoords"].as<glm::vec2>();
				spriteComponent.SpriteSize = spriteComponentNode["SpriteSize"].as<glm::vec2>();
				spriteComponent.SpriteSizeCoef = spriteComponentNode["SpriteSizeCoef"].as<glm::vec2>();

				if (spriteComponent.bSubTexture && spriteComponent.Material->DiffuseTexture)
				{
					spriteComponent.SubTexture = SubTexture2D::CreateFromCoords(Cast<Texture2D>(spriteComponent.Material->DiffuseTexture),
						spriteComponent.SubTextureCoords, spriteComponent.SpriteSize, spriteComponent.SpriteSizeCoef);
				}
			}

			spriteComponent.SetRelativeTransform(relativeTransform);
		}

		auto staticMeshComponentNode = entityNode["StaticMeshComponent"];
		if (staticMeshComponentNode)
		{
			auto& smComponent = deserializedEntity.AddComponent<StaticMeshComponent>();
			auto& sm = smComponent.StaticMesh;
			Transform relativeTransform;

			std::filesystem::path smPath = staticMeshComponentNode["Path"].as<std::string>();
			uint32_t meshIndex = 0u;
			bool bImportAsSingleFileIfPossible = false;
			if (staticMeshComponentNode["Index"])
				meshIndex = staticMeshComponentNode["Index"].as<uint32_t>();
			if (staticMeshComponentNode["MadeOfMultipleMeshes"])
				bImportAsSingleFileIfPossible = staticMeshComponentNode["MadeOfMultipleMeshes"].as<bool>();

			if (StaticMeshLibrary::Get(smPath, &sm, meshIndex) == false)
			{
				sm = StaticMesh::Create(smPath, true, bImportAsSingleFileIfPossible, false);
			}
			auto& material = sm->Material;

			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);

			if (auto materialNode = staticMeshComponentNode["Material"])
				DeserializeMaterial(materialNode, material);

			smComponent.SetRelativeTransform(relativeTransform);
		}

		auto pointLightComponentNode = entityNode["PointLightComponent"];
		if (pointLightComponentNode)
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(pointLightComponentNode, relativeTransform);

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

			DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);

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

			DeserializeRelativeTransform(spotLightComponentNode, relativeTransform);

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

		auto scriptComponentNode = entityNode["ScriptComponent"];
		if (scriptComponentNode)
		{
			auto& scriptComponent = deserializedEntity.AddComponent<ScriptComponent>();

			scriptComponent.ModuleName = scriptComponentNode["ModuleName"].as<std::string>();
		}
	}

	void SceneSerializer::DeserializeSkybox(YAML::Node& node)
	{
		bool bAsSRGB = true;
		auto skyboxNode = node["Skybox"];
		if (skyboxNode)
		{
			const char* sides[] = { "Right", "Left", "Top", "Bottom", "Front", "Back" };
			std::array<Ref<Texture>, 6> textures;
			
			for (int i = 0; i < textures.size(); ++i)
			{
				if (auto textureNode = skyboxNode[sides[i]])
				{
					const std::filesystem::path& path = textureNode["Path"].as<std::string>();
					auto sRGBNode = textureNode["sRGB"];
					if (sRGBNode)
						bAsSRGB = sRGBNode.as<bool>();

					if (path == "White")
						textures[i] = Texture2D::WhiteTexture;
					else if (path == "Black")
						textures[i] = Texture2D::BlackTexture;
					else
					{
						Ref<Texture> texture;
						if (TextureLibrary::Get(path, &texture))
						{
							textures[i] = texture;
						}
						else
						{
							textures[i] = Texture2D::Create(path, bAsSRGB);
						}
					}
				}
			}
			
			m_Scene->m_Cubemap = Cubemap::Create(textures);

			if (skyboxNode["Enabled"])
			{
				m_Scene->SetEnableSkybox(skyboxNode["Enabled"].as<bool>());
			}
		}
	}

	void SceneSerializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		relativeTransform.Translation = node["RelativeTranslation"].as<glm::vec3>();
		relativeTransform.Rotation = node["RelativeRotation"].as<glm::vec3>();
		relativeTransform.Scale3D = node["RelativeScale"].as<glm::vec3>();
	}

	void SceneSerializer::DeserializeMaterial(YAML::Node& materialNode, Ref<Material>& material)
	{
		DeserializeTexture(materialNode, material->DiffuseTexture, "DiffuseTexture");
		DeserializeTexture(materialNode, material->SpecularTexture, "SpecularTexture");
		DeserializeTexture(materialNode, material->NormalTexture, "NormalTexture");

		if (materialNode["Shader"])
		{
			const std::filesystem::path& path = materialNode["Shader"].as<std::string>();
			material->Shader = ShaderLibrary::GetOrLoad(path);
		}

		auto tintColorNode = materialNode["TintColor"];
		if (tintColorNode)
			material->TintColor = materialNode["TintColor"].as<glm::vec4>();

		auto tilingFactorNode = materialNode["TilingFactor"];
		if (tilingFactorNode)
			material->TilingFactor = materialNode["TilingFactor"].as<float>();
		material->Shininess = materialNode["Shininess"].as<float>();
	}

	void SceneSerializer::DeserializeTexture(YAML::Node& parentNode, Ref<Texture>& texture, const std::string& textureName)
	{
		bool bAsSRGB = true;

		if (auto textureNode = parentNode[textureName])
		{
			const std::filesystem::path& path = textureNode["Path"].as<std::string>();

			if (auto sRGBNode = textureNode["sRGB"])
				bAsSRGB = sRGBNode.as<bool>();

			if (path == "None")
			{}
			else if (path == "White")
				texture = Texture2D::WhiteTexture;
			else if (path == "Black")
				texture = Texture2D::BlackTexture;
			else
			{
				Ref<Texture> libTexture;
				if (TextureLibrary::Get(path, &libTexture))
				{
					texture = libTexture;
				}
				else
				{
					texture = Texture2D::Create(path, bAsSRGB);
				}
			}
		}
	}

}
