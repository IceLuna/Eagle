#include "egpch.h"

#include "SceneSerializer.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Renderer/Shader.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"

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
		EG_CORE_TRACE("Saving Scene at '{0}'", std::filesystem::absolute(filepath));

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

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			EG_CORE_WARN("Can't load scene {0}. File doesn't exist!", std::filesystem::absolute(filepath));
			return false;
		}
		
		YAML::Node data = YAML::LoadFile(filepath.string());
		if (!data["Scene"])
		{
			EG_CORE_WARN("Can't load scene {0}. File has invalid format!", std::filesystem::absolute(filepath));
			return false;
		}
		EG_CORE_TRACE("Loading scene '{0}'", std::filesystem::absolute(filepath));

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
			transform.Location = editorCameraNode["Location"].as<glm::vec3>();
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
				Entity& parent = m_AllEntities[element.second];
				Entity child((entt::entity)element.first, m_Scene.get());
				child.SetParent(parent);
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

			int parentID = -1;
			if (Entity& parent = entity.GetParent())
			{
				parentID = (int)parent.GetID();
			}

			out << YAML::Key << "EntitySceneParams";
			out << YAML::BeginMap; //EntitySceneName

			out << YAML::Key << "Name"  << YAML::Value << name;
			out << YAML::Key << "Parent" << YAML::Value << parentID;

			out << YAML::EndMap; //EntitySceneParams
		}

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& worldTransform = entity.GetWorldTransform();
			const auto& relativeTransform = entity.GetRelativeTransform();

			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; //TransformComponent

			out << YAML::Key << "WorldLocation"		<< YAML::Value << worldTransform.Location;
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

			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; //StaticMeshComponent

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());
			SerializeStaticMesh(out, sm);

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
			out << YAML::Key << "Intensity" << YAML::Value << pointLightComponent.Intensity;

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
			out << YAML::Key << "Intensity" << YAML::Value << spotLightComponent.Intensity;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			auto& scriptComponent = entity.GetComponent<ScriptComponent>();

			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap;

			out << YAML::Key << "ModuleName" << YAML::Value << scriptComponent.ModuleName;

			//Public Fields
			out << YAML::Key << "PublicFields";
			out << YAML::BeginMap;
			for (auto& it : scriptComponent.PublicFields)
			{
				PublicField& field = it.second;
				if (HasSerializableType(field))
					SerializePublicFieldValue(out, field);
			}
			out << YAML::EndMap;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidBodyComponent>())
		{
			auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

			out << YAML::Key << "RigidBodyComponent";
			out << YAML::BeginMap; //RigidBodyComponent

			SerializeRelativeTransform(out, rigidBodyComponent.GetRelativeTransform());

			out << YAML::Key << "BodyType" << YAML::Value << (int)rigidBodyComponent.BodyType;
			out << YAML::Key << "CollisionDetectionType" << YAML::Value << (int)rigidBodyComponent.CollisionDetection;
			out << YAML::Key << "Mass" << YAML::Value << rigidBodyComponent.Mass;
			out << YAML::Key << "LinearDamping" << YAML::Value << rigidBodyComponent.LinearDamping;
			out << YAML::Key << "AngularDamping" << YAML::Value << rigidBodyComponent.AngularDamping;
			out << YAML::Key << "EnableGravity" << YAML::Value << rigidBodyComponent.EnableGravity;
			out << YAML::Key << "IsKinematic" << YAML::Value << rigidBodyComponent.IsKinematic;
			out << YAML::Key << "LockPositionX" << YAML::Value << rigidBodyComponent.LockPositionX;
			out << YAML::Key << "LockPositionY" << YAML::Value << rigidBodyComponent.LockPositionY;
			out << YAML::Key << "LockPositionZ" << YAML::Value << rigidBodyComponent.LockPositionZ;
			out << YAML::Key << "LockRotationX" << YAML::Value << rigidBodyComponent.LockRotationX;
			out << YAML::Key << "LockRotationY" << YAML::Value << rigidBodyComponent.LockRotationY;
			out << YAML::Key << "LockRotationZ" << YAML::Value << rigidBodyComponent.LockRotationZ;

			out << YAML::EndMap; //RigidBodyComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			auto& collider = entity.GetComponent<BoxColliderComponent>();

			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; //BoxColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			SerializePhysicsMaterial(out, collider.Material);

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger;
			out << YAML::Key << "Size" << YAML::Value << collider.Size;
			out << YAML::EndMap; //BoxColliderComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			auto& collider = entity.GetComponent<SphereColliderComponent>();

			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; //SphereColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			SerializePhysicsMaterial(out, collider.Material);

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			out << YAML::EndMap; //SphereColliderComponent
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			auto& collider = entity.GetComponent<CapsuleColliderComponent>();

			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap; //CapsuleColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			SerializePhysicsMaterial(out, collider.Material);

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger;
			out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
			out << YAML::Key << "Height" << YAML::Value << collider.Height;
			out << YAML::EndMap; //CapsuleColliderComponent
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			auto& collider = entity.GetComponent<MeshColliderComponent>();

			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap; //MeshColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			SerializeStaticMesh(out, collider.CollisionMesh);
			SerializePhysicsMaterial(out, collider.Material);

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger;
			out << YAML::Key << "IsConvex" << YAML::Value << collider.IsConvex;
			out << YAML::EndMap; //MeshColliderComponent
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
		out << YAML::Key << "RelativeLocation" << YAML::Value << relativeTransform.Location;
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

	void SceneSerializer::SerializePhysicsMaterial(YAML::Emitter& out, const Ref<PhysicsMaterial>& material)
	{
		out << YAML::Key << "PhysicsMaterial";
		out << YAML::BeginMap; //PhysicsMaterial

		out << YAML::Key << "StaticFriction" << YAML::Value << material->StaticFriction;
		out << YAML::Key << "DynamicFriction" << YAML::Value << material->DynamicFriction;
		out << YAML::Key << "Bounciness" << YAML::Value << material->Bounciness;

		out << YAML::EndMap; //PhysicsMaterial
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

	void SceneSerializer::SerializeStaticMesh(YAML::Emitter& out, const Ref<StaticMesh>& staticMesh)
	{
		if (staticMesh)
		{
			std::filesystem::path currentPath = std::filesystem::current_path();
			std::filesystem::path smRelPath = std::filesystem::relative(staticMesh->GetPath(), currentPath);

			out << YAML::Key << "StaticMesh";
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << smRelPath.string();
			out << YAML::Key << "Index" << YAML::Value << staticMesh->GetIndex();
			out << YAML::Key << "MadeOfMultipleMeshes" << YAML::Value << staticMesh->IsMadeOfMultipleMeshes();

			SerializeMaterial(out, staticMesh->Material);
			out << YAML::EndMap;
		}
	}

	void SceneSerializer::DeserializeEntity(Ref<Scene>& scene, YAML::iterator::value_type& entityNode)
	{
		//TODO: Add tags serialization and deserialization
		const uint32_t id = entityNode["EntityID"].as<uint32_t>();

		std::string name;
		int parentID = -1;
		auto sceneNameComponentNode = entityNode["EntitySceneParams"];
		if (sceneNameComponentNode)
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

		auto transformComponentNode = entityNode["TransformComponent"];
		if (transformComponentNode)
		{
			//Every entity has a transform component
			Transform worldTransform;

			worldTransform.Location = transformComponentNode["WorldLocation"].as<glm::vec3>();
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

			if (auto node = staticMeshComponentNode["StaticMesh"])
				DeserializeStaticMesh(node, sm);

			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);

			smComponent.SetRelativeTransform(relativeTransform);
		}

		auto pointLightComponentNode = entityNode["PointLightComponent"];
		if (pointLightComponentNode)
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(pointLightComponentNode, relativeTransform);

			pointLightComponent.LightColor = pointLightComponentNode["LightColor"].as<glm::vec3>();
			pointLightComponent.Ambient = pointLightComponentNode["Ambient"].as<glm::vec3>();
			pointLightComponent.Specular = pointLightComponentNode["Specular"].as<glm::vec3>();
			if (pointLightComponentNode["Intensity"])
				pointLightComponent.Intensity = pointLightComponentNode["Intensity"].as<float>();

			pointLightComponent.SetRelativeTransform(relativeTransform);
		}

		auto directionalLightComponentNode = entityNode["DirectionalLightComponent"];
		if (directionalLightComponentNode)
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);

			directionalLightComponent.LightColor = directionalLightComponentNode["LightColor"].as<glm::vec3>();
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

			spotLightComponent.LightColor = spotLightComponentNode["LightColor"].as<glm::vec3>();
			spotLightComponent.Ambient = spotLightComponentNode["Ambient"].as<glm::vec3>();
			spotLightComponent.Specular = spotLightComponentNode["Specular"].as<glm::vec3>();

			if (spotLightComponentNode["InnerCutOffAngle"])
			{
				spotLightComponent.InnerCutOffAngle = spotLightComponentNode["InnerCutOffAngle"].as<float>();
				spotLightComponent.OuterCutOffAngle = spotLightComponentNode["OuterCutOffAngle"].as<float>();
			}
			if (spotLightComponentNode["Intensity"])
				spotLightComponent.Intensity = spotLightComponentNode["Intensity"].as<float>();

			spotLightComponent.SetRelativeTransform(relativeTransform);
		}

		auto scriptComponentNode = entityNode["ScriptComponent"];
		if (scriptComponentNode)
		{
			auto& scriptComponent = deserializedEntity.AddComponent<ScriptComponent>();

			scriptComponent.ModuleName = scriptComponentNode["ModuleName"].as<std::string>();
			ScriptEngine::InitEntityScript(deserializedEntity);

			auto publicFieldsNode = scriptComponentNode["PublicFields"];
			if (publicFieldsNode)
				DeserializePublicFieldValues(publicFieldsNode, scriptComponent);
		}
	
		auto rigidBodyComponentNode = entityNode["RigidBodyComponent"];
		if (rigidBodyComponentNode)
		{
			auto& rigidBodyComponent = deserializedEntity.AddComponent<RigidBodyComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(rigidBodyComponentNode, relativeTransform);

			rigidBodyComponent.BodyType = RigidBodyComponent::Type(rigidBodyComponentNode["BodyType"].as<int>());
			rigidBodyComponent.CollisionDetection = RigidBodyComponent::CollisionDetectionType(rigidBodyComponentNode["CollisionDetectionType"].as<int>());
			rigidBodyComponent.Mass = rigidBodyComponentNode["Mass"].as<float>();
			rigidBodyComponent.LinearDamping = rigidBodyComponentNode["LinearDamping"].as<float>();
			rigidBodyComponent.AngularDamping = rigidBodyComponentNode["AngularDamping"].as<float>();
			rigidBodyComponent.EnableGravity = rigidBodyComponentNode["EnableGravity"].as<bool>();
			rigidBodyComponent.IsKinematic = rigidBodyComponentNode["IsKinematic"].as<bool>();
			rigidBodyComponent.LockPositionX = rigidBodyComponentNode["LockPositionX"].as<bool>();
			rigidBodyComponent.LockPositionY = rigidBodyComponentNode["LockPositionY"].as<bool>();
			rigidBodyComponent.LockPositionZ = rigidBodyComponentNode["LockPositionZ"].as<bool>();
			rigidBodyComponent.LockRotationX = rigidBodyComponentNode["LockRotationX"].as<bool>();
			rigidBodyComponent.LockRotationY = rigidBodyComponentNode["LockRotationY"].as<bool>();
			rigidBodyComponent.LockRotationZ = rigidBodyComponentNode["LockRotationZ"].as<bool>();
			
			rigidBodyComponent.SetRelativeTransform(relativeTransform);
		}
	
		auto boxColliderNode = entityNode["BoxColliderComponent"];
		if (boxColliderNode)
		{
			auto& collider = deserializedEntity.AddComponent<BoxColliderComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(boxColliderNode, relativeTransform);
			if (auto node = boxColliderNode["PhysicsMaterial"])
				DeserializePhysicsMaterial(node, collider.Material);

			collider.IsTrigger = boxColliderNode["IsTrigger"].as<bool>();
			collider.Size = boxColliderNode["Size"].as<glm::vec3>();

			collider.SetRelativeTransform(relativeTransform);
		}
	
		auto sphereColliderNode = entityNode["SphereColliderComponent"];
		if (sphereColliderNode)
		{
			auto& collider = deserializedEntity.AddComponent<SphereColliderComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(sphereColliderNode, relativeTransform);
			if (auto node = sphereColliderNode["PhysicsMaterial"])
				DeserializePhysicsMaterial(node, collider.Material);

			collider.IsTrigger = sphereColliderNode["IsTrigger"].as<bool>();
			collider.Radius = sphereColliderNode["Radius"].as<float>();

			collider.SetRelativeTransform(relativeTransform);
		}
	
		auto capsuleColliderNode = entityNode["CapsuleColliderComponent"];
		if (capsuleColliderNode)
		{
			auto& collider = deserializedEntity.AddComponent<CapsuleColliderComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(capsuleColliderNode, relativeTransform);
			if (auto node = capsuleColliderNode["PhysicsMaterial"])
				DeserializePhysicsMaterial(node, collider.Material);

			collider.IsTrigger = capsuleColliderNode["IsTrigger"].as<bool>();
			collider.Radius = capsuleColliderNode["Radius"].as<float>();
			collider.Height = capsuleColliderNode["Height"].as<float>();

			collider.SetRelativeTransform(relativeTransform);
		}
	
		auto meshColliderNode = entityNode["MeshColliderComponent"];
		if (meshColliderNode)
		{
			auto& collider = deserializedEntity.AddComponent<MeshColliderComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(meshColliderNode, relativeTransform);
			if (auto node = meshColliderNode["StaticMesh"])
				DeserializeStaticMesh(node, collider.CollisionMesh);
			if (auto node = meshColliderNode["PhysicsMaterial"])
				DeserializePhysicsMaterial(node, collider.Material);

			collider.IsTrigger = meshColliderNode["IsTrigger"].as<bool>();
			collider.IsConvex = meshColliderNode["IsConvex"].as<bool>();

			collider.SetRelativeTransform(relativeTransform);
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
		relativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
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

	void SceneSerializer::DeserializePhysicsMaterial(YAML::Node& materialNode, Ref<PhysicsMaterial>& material)
	{
		if (materialNode["StaticFriction"])
		{
			float staticFriction = materialNode["StaticFriction"].as<float>();
			material->StaticFriction = staticFriction;
		}

		if (materialNode["DynamicFriction"])
		{
			float dynamicFriction = materialNode["DynamicFriction"].as<float>();
			material->DynamicFriction = dynamicFriction;
		}

		if (materialNode["Bounciness"])
		{
			float bounciness = materialNode["Bounciness"].as<float>();
			material->Bounciness = bounciness;
		}
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

	void SceneSerializer::DeserializeStaticMesh(YAML::Node& meshNode, Ref<StaticMesh>& staticMesh)
	{
		std::filesystem::path smPath = meshNode["Path"].as<std::string>();
		uint32_t meshIndex = 0u;
		bool bImportAsSingleFileIfPossible = false;
		if (meshNode["Index"])
			meshIndex = meshNode["Index"].as<uint32_t>();
		if (meshNode["MadeOfMultipleMeshes"])
			bImportAsSingleFileIfPossible = meshNode["MadeOfMultipleMeshes"].as<bool>();

		if (StaticMeshLibrary::Get(smPath, &staticMesh, meshIndex) == false)
		{
			staticMesh = StaticMesh::Create(smPath, true, bImportAsSingleFileIfPossible, false);
		}

		if (auto materialNode = meshNode["Material"])
			DeserializeMaterial(materialNode, staticMesh->Material);
	}

	void SceneSerializer::SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Key << field.Name;
		switch (field.Type)
		{
			case FieldType::Int:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<int>() << YAML::EndSeq;
				break;
			}
			case FieldType::UnsignedInt:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<unsigned int>() << YAML::EndSeq;
				break;
			}
			case FieldType::Float:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<float>() << YAML::EndSeq;
				break;
			}
			case FieldType::String:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<std::string>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec2:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec2>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec3:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec3>() << YAML::EndSeq;
				break;
			}
			case FieldType::Vec4:
			{
				out << YAML::Value << YAML::BeginSeq << (uint32_t)field.Type << field.GetStoredValue<glm::vec4>() << YAML::EndSeq;
				break;
			}
		}
	}

	void SceneSerializer::DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent)
	{
		auto& publicFields = scriptComponent.PublicFields;
		for (auto& it : publicFieldsNode)
		{
			std::string fieldName = it.first.as<std::string>();
			FieldType fieldType = (FieldType)it.second[0].as<uint32_t>();

			auto& fieldIt = publicFields.find(fieldName);
			if ((fieldIt != publicFields.end()) && (fieldType == fieldIt->second.Type))
			{
				PublicField& field = fieldIt->second;
				switch (fieldType)
				{
					case FieldType::Int:
					{
						int value = it.second[1].as<int>();
						field.SetStoredValue<int>(value);
						break;
					}
					case FieldType::UnsignedInt:
					{
						unsigned int value = it.second[1].as<unsigned int>();
						field.SetStoredValue<unsigned int>(value); 
						break;
					}
					case FieldType::Float:
					{
						float value = it.second[1].as<float>();
						field.SetStoredValue<float>(value);
						break;
					}
					case FieldType::String:
					{
						std::string value = it.second[1].as<std::string>();
						field.SetStoredValue<std::string>(value);
						break;
					}
					case FieldType::Vec2:
					{
						glm::vec2 value = it.second[1].as<glm::vec2>();
						field.SetStoredValue<glm::vec2>(value);
						break;
					}
					case FieldType::Vec3:
					{
						glm::vec3 value = it.second[1].as<glm::vec3>();
						field.SetStoredValue<glm::vec3>(value);
						break;
					}
					case FieldType::Vec4:
					{
						glm::vec4 value = it.second[1].as<glm::vec4>();
						field.SetStoredValue<glm::vec4>(value);
						break;
					}
				}
			}
		}
	}

	bool SceneSerializer::HasSerializableType(const PublicField& field)
	{
		switch (field.Type)
		{
			case FieldType::Int: return true;
			case FieldType::UnsignedInt: return true;
			case FieldType::Float: return true;
			case FieldType::String: return true;
			case FieldType::Vec2: return true;
			case FieldType::Vec3: return true;
			case FieldType::Vec4: return true;
			default: return false;
		}
	}

}
