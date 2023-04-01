#include "egpch.h"

#include "SceneSerializer.h"
#include "Serializer.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Camera/CameraController.h"
#include "Eagle/Script/ScriptEngine.h"
#include "Eagle/Physics/PhysicsMaterial.h"

namespace Eagle
{
	SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene)
	{}

	bool SceneSerializer::Serialize(const Path& filepath)
	{
		EG_CORE_TRACE("Saving Scene at '{0}'", std::filesystem::absolute(filepath));

		const auto& rendererSettings = m_Scene->GetSceneRenderer()->GetOptions();
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene"	<< YAML::Value << "Untitled";
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Gamma" << YAML::Value << rendererSettings.Gamma;
		out << YAML::Key << "Exposure" << YAML::Value << rendererSettings.Exposure;
		out << YAML::Key << "TonemappingMethod" << YAML::Value << (uint32_t)rendererSettings.Tonemapping;

		const auto& photoLinearParams = rendererSettings.PhotoLinearTonemappingParams;
		out << YAML::Key << "PhotoLinearTonemappingSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Sensetivity" << YAML::Value << photoLinearParams.Sensetivity;
		out << YAML::Key << "ExposureTime" << YAML::Value << photoLinearParams.ExposureTime;
		out << YAML::Key << "FStop" << YAML::Value << photoLinearParams.FStop;
		out << YAML::EndMap; //PhotoLinearTonemappingSettings

		const auto& filmicParams = rendererSettings.FilmicTonemappingParams;
		out << YAML::Key << "FilmicTonemappingSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "WhitePoint" << YAML::Value << filmicParams.WhitePoint;
		out << YAML::EndMap; //FilmicTonemappingSettings

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

	bool SceneSerializer::Deserialize(const Path& filepath)
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

		SceneRendererSettings rendererSettings = m_Scene->GetSceneRenderer()->GetOptions();
		if (auto gammaNode = data["Gamma"])
			rendererSettings.Gamma = gammaNode.as<float>();
		if (auto exposureNode = data["Exposure"])
			rendererSettings.Exposure = exposureNode.as<float>();
		if (auto tonemappingNode = data["TonemappingMethod"])
			rendererSettings.Tonemapping = TonemappingMethod(tonemappingNode.as<uint32_t>());

		if (auto photolinearNode = data["PhotoLinearTonemappingSettings"])
		{
			PhotoLinearTonemappingSettings params;
			params.Sensetivity = photolinearNode["Sensetivity"].as<float>();
			params.ExposureTime = photolinearNode["ExposureTime"].as<float>();
			params.FStop = photolinearNode["FStop"].as<float>();

			rendererSettings.PhotoLinearTonemappingParams = params;
		}

		if (auto filmicNode = data["FilmicTonemappingSettings"])
		{
			FilmicTonemappingSettings params;
			params.WhitePoint = filmicNode["WhitePoint"].as<float>();

			rendererSettings.FilmicTonemappingParams = params;
		}
		
		if (auto editorCameraNode = data["EditorCamera"])
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

		m_Scene->GetSceneRenderer()->SetOptions(rendererSettings);
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

		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			auto& sceneNameComponent = entity.GetComponent<EntitySceneNameComponent>();
			auto& name = sceneNameComponent.Name;

			int parentID = -1;
			if (Entity& parent = entity.GetParent())
				parentID = (int)parent.GetID();

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

			Serializer::SerializeMaterial(out, material);

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<BillboardComponent>())
		{
			auto& component = entity.GetComponent<BillboardComponent>();

			out << YAML::Key << "BillboardComponent";
			out << YAML::BeginMap; //BillboardComponent

			SerializeRelativeTransform(out, component.GetRelativeTransform());
			Serializer::SerializeTexture(out, component.Texture, "Texture");

			out << YAML::EndMap; //BillboardComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			auto& smComponent = entity.GetComponent<StaticMeshComponent>();
			auto& sm = smComponent.GetStaticMesh();

			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; //StaticMeshComponent

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());
			Serializer::SerializeStaticMesh(out, sm);
			Serializer::SerializeMaterial(out, smComponent.Material);

			out << YAML::EndMap; //StaticMeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, pointLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << pointLightComponent.GetLightColor();
			out << YAML::Key << "Intensity" << YAML::Value << pointLightComponent.GetIntensity();
			out << YAML::Key << "AffectsWorld" << YAML::Value << pointLightComponent.DoesAffectWorld();

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, directionalLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.LightColor;
			out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.Intensity;
			out << YAML::Key << "AffectsWorld" << YAML::Value << directionalLightComponent.bAffectsWorld;

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();

			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spotLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << spotLightComponent.GetLightColor();
			out << YAML::Key << "InnerCutOffAngle" << YAML::Value << spotLightComponent.GetInnerCutOffAngle();
			out << YAML::Key << "OuterCutOffAngle" << YAML::Value << spotLightComponent.GetOuterCutOffAngle();
			out << YAML::Key << "Intensity" << YAML::Value << spotLightComponent.GetIntensity();
			out << YAML::Key << "AffectsWorld" << YAML::Value << spotLightComponent.DoesAffectWorld();

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
				if (Serializer::HasSerializableType(field))
					Serializer::SerializePublicFieldValue(out, field);
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
			out << YAML::Key << "Mass" << YAML::Value << rigidBodyComponent.GetMass();
			out << YAML::Key << "LinearDamping" << YAML::Value << rigidBodyComponent.GetLinearDamping();
			out << YAML::Key << "AngularDamping" << YAML::Value << rigidBodyComponent.GetAngularDamping();
			out << YAML::Key << "EnableGravity" << YAML::Value << rigidBodyComponent.IsGravityEnabled();
			out << YAML::Key << "IsKinematic" << YAML::Value << rigidBodyComponent.IsKinematic();
			out << YAML::Key << "LockPositionX" << YAML::Value << rigidBodyComponent.IsPositionXLocked();
			out << YAML::Key << "LockPositionY" << YAML::Value << rigidBodyComponent.IsPositionYLocked();
			out << YAML::Key << "LockPositionZ" << YAML::Value << rigidBodyComponent.IsPositionZLocked();
			out << YAML::Key << "LockRotationX" << YAML::Value << rigidBodyComponent.IsRotationXLocked();
			out << YAML::Key << "LockRotationY" << YAML::Value << rigidBodyComponent.IsRotationYLocked();
			out << YAML::Key << "LockRotationZ" << YAML::Value << rigidBodyComponent.IsRotationZLocked();

			out << YAML::EndMap; //RigidBodyComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			auto& collider = entity.GetComponent<BoxColliderComponent>();

			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; //BoxColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			Serializer::SerializePhysicsMaterial(out, collider.GetPhysicsMaterial());

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Size" << YAML::Value << collider.GetSize();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::EndMap; //BoxColliderComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			auto& collider = entity.GetComponent<SphereColliderComponent>();

			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; //SphereColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			Serializer::SerializePhysicsMaterial(out, collider.GetPhysicsMaterial());

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::EndMap; //SphereColliderComponent
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			auto& collider = entity.GetComponent<CapsuleColliderComponent>();

			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap; //CapsuleColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			Serializer::SerializePhysicsMaterial(out, collider.GetPhysicsMaterial());

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "Radius" << YAML::Value << collider.GetRadius();
			out << YAML::Key << "Height" << YAML::Value << collider.GetHeight();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::EndMap; //CapsuleColliderComponent
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			auto& collider = entity.GetComponent<MeshColliderComponent>();

			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap; //MeshColliderComponent

			SerializeRelativeTransform(out, collider.GetRelativeTransform());
			Serializer::SerializeStaticMesh(out, collider.GetCollisionMesh());
			Serializer::SerializePhysicsMaterial(out, collider.GetPhysicsMaterial());

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsConvex" << YAML::Value << collider.IsConvex();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::EndMap; //MeshColliderComponent
		}

		if (entity.HasComponent<AudioComponent>())
		{
			auto& audio = entity.GetComponent<AudioComponent>();

			out << YAML::Key << "AudioComponent";
			out << YAML::BeginMap; //AudioComponent

			SerializeRelativeTransform(out, audio.GetRelativeTransform());
			Serializer::SerializeSound(out, audio.GetSound());

			out << YAML::Key << "Volume" << YAML::Value << audio.GetVolume();
			out << YAML::Key << "LoopCount" << YAML::Value << audio.GetLoopCount();
			out << YAML::Key << "IsLooping" << YAML::Value << audio.IsLooping();
			out << YAML::Key << "IsMuted" << YAML::Value << audio.IsMuted();
			out << YAML::Key << "IsStreaming" << YAML::Value << audio.IsStreaming();
			out << YAML::Key << "MinDistance" << YAML::Value << audio.GetMinDistance();
			out << YAML::Key << "MaxDistance" << YAML::Value << audio.GetMaxDistance();
			out << YAML::Key << "RollOff" << YAML::Value << (uint32_t)audio.GetRollOffModel();
			out << YAML::Key << "Autoplay" << YAML::Value << audio.bAutoplay;
			out << YAML::Key << "EnableDopplerEffect" << YAML::Value << audio.bEnableDopplerEffect;

			out << YAML::EndMap; //AudioComponent
		}

		if (entity.HasComponent<ReverbComponent>())
		{
			auto& reverb = entity.GetComponent<ReverbComponent>();

			out << YAML::Key << "ReverbComponent";
			out << YAML::BeginMap; //ReverbComponent

			SerializeRelativeTransform(out, reverb.GetRelativeTransform());
			Serializer::SerializeReverb(out, reverb.Reverb);

			out << YAML::EndMap; //ReverbComponent
		}

		out << YAML::EndMap; //Entity
	}

	void SceneSerializer::SerializeSkybox(YAML::Emitter& out)
	{
		//Skybox
		out << YAML::Key << "IBL" << YAML::BeginMap;
		if (const Ref<TextureCube>& ibl = m_Scene->GetSceneRenderer()->GetSkybox())
		{
			Path currentPath = std::filesystem::current_path();
			Path texturePath = std::filesystem::relative(ibl->GetPath(), currentPath);

			out << YAML::Key << "Path" << YAML::Value << texturePath.string();
			out << YAML::Key << "Size" << YAML::Value << ibl->GetSize().x;
		}
		out << YAML::EndMap; //IBL
	}

	void SceneSerializer::SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform)
	{
		out << YAML::Key << "RelativeLocation" << YAML::Value << relativeTransform.Location;
		out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
		out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;
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

		if (auto transformComponentNode = entityNode["TransformComponent"])
		{
			//Every entity has a transform component
			Transform worldTransform;

			worldTransform.Location = transformComponentNode["WorldLocation"].as<glm::vec3>();
			worldTransform.Rotation = transformComponentNode["WorldRotation"].as<Rotator>();
			worldTransform.Scale3D = transformComponentNode["WorldScale"].as<glm::vec3>();

			deserializedEntity.SetWorldTransform(worldTransform);
		}

		if (auto cameraComponentNode = entityNode["CameraComponent"])
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

		if (auto spriteComponentNode = entityNode["SpriteComponent"])
		{
			auto& spriteComponent = deserializedEntity.AddComponent<SpriteComponent>();
			auto& material = spriteComponent.Material;
			Transform relativeTransform;

			DeserializeRelativeTransform(spriteComponentNode, relativeTransform);
			if (auto materialNode = spriteComponentNode["Material"])
				Serializer::DeserializeMaterial(materialNode, material);

			auto subtextureNode = spriteComponentNode["bSubTexture"];
			if (subtextureNode)
			{
				spriteComponent.bSubTexture = spriteComponentNode["bSubTexture"].as<bool>();
				spriteComponent.SubTextureCoords = spriteComponentNode["SubTextureCoords"].as<glm::vec2>();
				spriteComponent.SpriteSize = spriteComponentNode["SpriteSize"].as<glm::vec2>();
				spriteComponent.SpriteSizeCoef = spriteComponentNode["SpriteSizeCoef"].as<glm::vec2>();

				auto& albedo = spriteComponent.Material->GetAlbedoTexture();
				if (spriteComponent.bSubTexture && albedo)
				{
					spriteComponent.SubTexture = SubTexture2D::CreateFromCoords(Cast<Texture2D>(albedo),
						spriteComponent.SubTextureCoords, spriteComponent.SpriteSize, spriteComponent.SpriteSizeCoef);
				}
			}

			spriteComponent.SetRelativeTransform(relativeTransform);
		}

		if (auto billboardComponentNode = entityNode["BillboardComponent"])
		{
			auto& billboardComponent = deserializedEntity.AddComponent<BillboardComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(billboardComponentNode, relativeTransform);
			Serializer::DeserializeTexture2D(billboardComponentNode, billboardComponent.Texture, "Texture");

			billboardComponent.SetRelativeTransform(relativeTransform);
		}

		if (auto staticMeshComponentNode = entityNode["StaticMeshComponent"])
		{
			auto& smComponent = deserializedEntity.AddComponent<StaticMeshComponent>();
			Ref<StaticMesh> sm;

			Transform relativeTransform;
			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto node = staticMeshComponentNode["StaticMesh"])
			{
				Serializer::DeserializeStaticMesh(node, sm);
				smComponent.SetStaticMesh(sm);
			}
			if (auto materialNode = staticMeshComponentNode["Material"])
				Serializer::DeserializeMaterial(materialNode, smComponent.Material);
		}

		if (auto pointLightComponentNode = entityNode["PointLightComponent"])
		{
			auto& pointLightComponent = deserializedEntity.AddComponent<PointLightComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(pointLightComponentNode, relativeTransform);
			pointLightComponent.SetRelativeTransform(relativeTransform);

			pointLightComponent.SetLightColor(pointLightComponentNode["LightColor"].as<glm::vec3>());
			if (auto node = pointLightComponentNode["Intensity"])
				pointLightComponent.SetIntensity(node.as<float>());
			if (auto node = pointLightComponentNode["AffectsWorld"])
				pointLightComponent.SetAffectsWorld(node.as<bool>());
		}

		if (auto directionalLightComponentNode = entityNode["DirectionalLightComponent"])
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);
			directionalLightComponent.SetRelativeTransform(relativeTransform);

			directionalLightComponent.LightColor = directionalLightComponentNode["LightColor"].as<glm::vec3>();
			if (auto intensityNode = directionalLightComponentNode["Intensity"])
				directionalLightComponent.Intensity = intensityNode.as<float>();
			if (auto node = directionalLightComponentNode["AffectsWorld"])
				directionalLightComponent.bAffectsWorld = node.as<bool>();
		}

		if (auto spotLightComponentNode = entityNode["SpotLightComponent"])
		{
			auto& spotLightComponent = deserializedEntity.AddComponent<SpotLightComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(spotLightComponentNode, relativeTransform);
			spotLightComponent.SetRelativeTransform(relativeTransform);

			spotLightComponent.SetLightColor(spotLightComponentNode["LightColor"].as<glm::vec3>());

			if (auto node = spotLightComponentNode["InnerCutOffAngle"])
			{
				spotLightComponent.SetInnerCutOffAngle(node.as<float>());
				spotLightComponent.SetOuterCutOffAngle(spotLightComponentNode["OuterCutOffAngle"].as<float>());
			}
			if (auto node = spotLightComponentNode["Intensity"])
				spotLightComponent.SetIntensity(node.as<float>());
			if (auto node = spotLightComponentNode["AffectsWorld"])
				spotLightComponent.SetAffectsWorld(node.as<bool>());
		}

		if (auto scriptComponentNode = entityNode["ScriptComponent"])
		{
			auto& scriptComponent = deserializedEntity.AddComponent<ScriptComponent>();

			scriptComponent.ModuleName = scriptComponentNode["ModuleName"].as<std::string>();
			ScriptEngine::InitEntityScript(deserializedEntity);

			auto publicFieldsNode = scriptComponentNode["PublicFields"];
			if (publicFieldsNode)
				Serializer::DeserializePublicFieldValues(publicFieldsNode, scriptComponent);
		}
	
		if (auto rigidBodyComponentNode = entityNode["RigidBodyComponent"])
		{
			auto& rigidBodyComponent = deserializedEntity.AddComponent<RigidBodyComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(rigidBodyComponentNode, relativeTransform);
			rigidBodyComponent.SetRelativeTransform(relativeTransform);

			rigidBodyComponent.BodyType = RigidBodyComponent::Type(rigidBodyComponentNode["BodyType"].as<int>());
			rigidBodyComponent.CollisionDetection = RigidBodyComponent::CollisionDetectionType(rigidBodyComponentNode["CollisionDetectionType"].as<int>());
			rigidBodyComponent.SetMass(rigidBodyComponentNode["Mass"].as<float>());
			rigidBodyComponent.SetLinearDamping(rigidBodyComponentNode["LinearDamping"].as<float>());
			rigidBodyComponent.SetAngularDamping(rigidBodyComponentNode["AngularDamping"].as<float>());
			rigidBodyComponent.SetEnableGravity(rigidBodyComponentNode["EnableGravity"].as<bool>());
			rigidBodyComponent.SetIsKinematic(rigidBodyComponentNode["IsKinematic"].as<bool>());
			rigidBodyComponent.SetLockPositionX(rigidBodyComponentNode["LockPositionX"].as<bool>());
			rigidBodyComponent.SetLockPositionY(rigidBodyComponentNode["LockPositionY"].as<bool>());
			rigidBodyComponent.SetLockPositionZ(rigidBodyComponentNode["LockPositionZ"].as<bool>());
			rigidBodyComponent.SetLockRotationX(rigidBodyComponentNode["LockRotationX"].as<bool>());
			rigidBodyComponent.SetLockRotationY(rigidBodyComponentNode["LockRotationY"].as<bool>());
			rigidBodyComponent.SetLockRotationZ(rigidBodyComponentNode["LockRotationZ"].as<bool>());
		}
	
		if (auto boxColliderNode = entityNode["BoxColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<BoxColliderComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(boxColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);
			
			Ref<PhysicsMaterial> material = MakeRef<PhysicsMaterial>();

			if (auto node = boxColliderNode["PhysicsMaterial"])
				Serializer::DeserializePhysicsMaterial(node, material);

			collider.SetPhysicsMaterial(material);
			collider.SetIsTrigger(boxColliderNode["IsTrigger"].as<bool>());
			collider.SetSize(boxColliderNode["Size"].as<glm::vec3>());
			collider.SetShowCollision(boxColliderNode["IsCollisionVisible"].as<bool>());
		}
	
		if (auto sphereColliderNode = entityNode["SphereColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<SphereColliderComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(sphereColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);
			
			Ref<PhysicsMaterial> material = MakeRef<PhysicsMaterial>();

			if (auto node = sphereColliderNode["PhysicsMaterial"])
				Serializer::DeserializePhysicsMaterial(node, material);

			collider.SetPhysicsMaterial(material);
			collider.SetIsTrigger(sphereColliderNode["IsTrigger"].as<bool>());
			collider.SetRadius(sphereColliderNode["Radius"].as<float>());
			collider.SetShowCollision(sphereColliderNode["IsCollisionVisible"].as<bool>());
		}
	
		if (auto capsuleColliderNode = entityNode["CapsuleColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<CapsuleColliderComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(capsuleColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			Ref<PhysicsMaterial> material = MakeRef<PhysicsMaterial>();

			if (auto node = capsuleColliderNode["PhysicsMaterial"])
				Serializer::DeserializePhysicsMaterial(node, material);

			collider.SetPhysicsMaterial(material);
			collider.SetIsTrigger(capsuleColliderNode["IsTrigger"].as<bool>());
			collider.SetRadius(capsuleColliderNode["Radius"].as<float>());
			collider.SetHeight(capsuleColliderNode["Height"].as<float>());
			collider.SetShowCollision(capsuleColliderNode["IsCollisionVisible"].as<bool>());
		}
	
		if (auto meshColliderNode = entityNode["MeshColliderComponent"])
		{
			auto& collider = deserializedEntity.AddComponent<MeshColliderComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(meshColliderNode, relativeTransform);
			collider.SetRelativeTransform(relativeTransform);

			Ref<PhysicsMaterial> material = MakeRef<PhysicsMaterial>();
			if (auto node = meshColliderNode["PhysicsMaterial"])
				Serializer::DeserializePhysicsMaterial(node, material);

			collider.SetPhysicsMaterial(material);
			collider.SetIsTrigger(meshColliderNode["IsTrigger"].as<bool>());
			collider.SetShowCollision(meshColliderNode["IsCollisionVisible"].as<bool>());
			collider.SetIsConvex(meshColliderNode["IsConvex"].as<bool>());

			Ref<StaticMesh> collisionMesh;
			if (auto node = meshColliderNode["StaticMesh"])
			{
				Serializer::DeserializeStaticMesh(node, collisionMesh);
				if (collisionMesh)
					collider.SetCollisionMesh(collisionMesh);
			}
		}
	
		if (auto audioNode = entityNode["AudioComponent"])
		{
			auto& audio = deserializedEntity.AddComponent<AudioComponent>();
			Path soundPath;
			
			Transform relativeTransform;
			DeserializeRelativeTransform(audioNode, relativeTransform);
			audio.SetRelativeTransform(relativeTransform);

			if (auto node = audioNode["Sound"])
				Serializer::DeserializeSound(node, soundPath);

			float volume = audioNode["Volume"].as<float>();
			int loopCount = audioNode["LoopCount"].as<int>();
			bool bLooping = audioNode["IsLooping"].as<bool>();
			bool bMuted = audioNode["IsMuted"].as<bool>();
			bool bStreaming = audioNode["IsStreaming"].as<bool>();
			float minDistance = audioNode["MinDistance"].as<float>();
			float maxDistance = audioNode["MaxDistance"].as<float>();
			RollOffModel rollOff = (RollOffModel)audioNode["RollOff"].as<uint32_t>();
			bool bAutoplay = audioNode["Autoplay"].as<bool>();
			bool bEnableDoppler = audioNode["EnableDopplerEffect"].as<bool>();

			audio.SetVolume(volume);
			audio.SetLoopCount(loopCount);
			audio.SetLooping(bLooping);
			audio.SetMuted(bMuted);
			audio.SetMinMaxDistance(minDistance, maxDistance);
			audio.SetRollOffModel(rollOff);
			audio.SetStreaming(bStreaming);
			audio.bAutoplay = bAutoplay;
			audio.bEnableDopplerEffect = bEnableDoppler;

			audio.SetSound(soundPath);
		}

		if (auto reverbNode = entityNode["ReverbComponent"])
		{
			auto& reverb = deserializedEntity.AddComponent<ReverbComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(reverbNode, relativeTransform);
			reverb.SetRelativeTransform(relativeTransform);

			if (auto node = reverbNode["Reverb"])
				Serializer::DeserializeReverb(node, reverb.Reverb);
		}
	}

	void SceneSerializer::DeserializeSkybox(YAML::Node& node)
	{
		if (auto skyboxNode = node["IBL"])
		{
			if (auto iblImageNode = skyboxNode["Path"])
			{
				const Path& path = iblImageNode.as<std::string>();
				uint32_t layerSize = TextureCube::SkyboxSize;
				if (auto iblImageSize = skyboxNode["Size"])
					layerSize = iblImageSize.as<uint32_t>();

				m_Scene->GetSceneRenderer()->SetSkybox(TextureCube::Create(path, layerSize));
			}
		}
	}

	void SceneSerializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		relativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
		relativeTransform.Rotation = node["RelativeRotation"].as<Rotator>();
		relativeTransform.Scale3D = node["RelativeScale"].as<glm::vec3>();
	}
}
