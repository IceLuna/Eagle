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
		out << YAML::Key << "TonemappingMethod" << YAML::Value << Serializer::GetEnumName(rendererSettings.Tonemapping);

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
		out << YAML::Key << "ProjectionMode" << YAML::Value << Serializer::GetEnumName(camera.GetProjectionMode());
		out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
		out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
		out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
		out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
		out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
		out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
		out << YAML::Key << "ShadowFarClip" << YAML::Value << camera.GetShadowFarClip();
		out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();
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
			rendererSettings.Tonemapping = Serializer::GetEnumFromName<TonemappingMethod>(tonemappingNode.as<std::string>());

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

			camera.SetProjectionMode(Serializer::GetEnumFromName<CameraProjectionMode>(editorCameraNode["ProjectionMode"].as<std::string>()));

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
			out << YAML::Key << "WorldRotation"	    << YAML::Value << worldTransform.Rotation;
			out << YAML::Key << "WorldScale"	    << YAML::Value << worldTransform.Scale3D;

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

			out << YAML::Key << "ProjectionMode"			<< YAML::Value << Serializer::GetEnumName(camera.GetProjectionMode());
			out << YAML::Key << "PerspectiveVerticalFOV"	<< YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip"		<< YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip"		<< YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize"			<< YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip"		<< YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip"		<< YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "ShadowFarClip"             << YAML::Value << camera.GetShadowFarClip();
			out << YAML::Key << "CascadesSplitAlpha"        << YAML::Value << camera.GetCascadesSplitAlpha();

			out << YAML::EndMap; //Camera

			SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary"			<< YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio"	<< YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			auto& spriteComponent = entity.GetComponent<SpriteComponent>();
			auto& material = spriteComponent.GetMaterial();

			out << YAML::Key << "SpriteComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spriteComponent.GetRelativeTransform());
			
			Ref<Texture2D> atlas;
			if (spriteComponent.IsSubTexture()  && spriteComponent.GetSubTexture()->GetTexture())
				atlas = spriteComponent.GetSubTexture()->GetTexture();

			Serializer::SerializeTexture(out, atlas, "SubTexture");
			out << YAML::Key << "bSubTexture" << YAML::Value << spriteComponent.IsSubTexture();
			out << YAML::Key << "bCastsShadows" << YAML::Value << spriteComponent.DoesCastShadows();
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

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());
			Serializer::SerializeStaticMesh(out, sm);
			Serializer::SerializeMaterial(out, smComponent.GetMaterial());

			out << YAML::EndMap; //StaticMeshComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			auto& pointLightComponent = entity.GetComponent<PointLightComponent>();

			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; //PointLightComponent

			SerializeRelativeTransform(out, pointLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << pointLightComponent.GetLightColor();
			out << YAML::Key << "Intensity" << YAML::Value << pointLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << pointLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "Radius" << YAML::Value << pointLightComponent.GetRadius();
			out << YAML::Key << "AffectsWorld" << YAML::Value << pointLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << pointLightComponent.DoesCastShadows();
			out << YAML::Key << "VisualizeRadius" << YAML::Value << pointLightComponent.VisualizeRadiusEnabled();
			out << YAML::Key << "IsVolumetric" << YAML::Value << pointLightComponent.IsVolumetricLight();

			out << YAML::EndMap; //PointLightComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			auto& directionalLightComponent = entity.GetComponent<DirectionalLightComponent>();

			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; //DirectionalLightComponent

			SerializeRelativeTransform(out, directionalLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << directionalLightComponent.GetLightColor();
			out << YAML::Key << "Ambient" << YAML::Value << directionalLightComponent.Ambient;
			out << YAML::Key << "Intensity" << YAML::Value << directionalLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << directionalLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "AffectsWorld" << YAML::Value << directionalLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << directionalLightComponent.DoesCastShadows();
			out << YAML::Key << "IsVolumetric" << YAML::Value << directionalLightComponent.IsVolumetricLight();

			out << YAML::EndMap; //DirectionalLightComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			auto& spotLightComponent = entity.GetComponent<SpotLightComponent>();

			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap; //SpotLightComponent

			SerializeRelativeTransform(out, spotLightComponent.GetRelativeTransform());

			out << YAML::Key << "LightColor" << YAML::Value << spotLightComponent.GetLightColor();
			out << YAML::Key << "InnerCutOffAngle" << YAML::Value << spotLightComponent.GetInnerCutOffAngle();
			out << YAML::Key << "OuterCutOffAngle" << YAML::Value << spotLightComponent.GetOuterCutOffAngle();
			out << YAML::Key << "Intensity" << YAML::Value << spotLightComponent.GetIntensity();
			out << YAML::Key << "VolumetricFogIntensity" << YAML::Value << spotLightComponent.GetVolumetricFogIntensity();
			out << YAML::Key << "Distance" << YAML::Value << spotLightComponent.GetDistance();
			out << YAML::Key << "AffectsWorld" << YAML::Value << spotLightComponent.DoesAffectWorld();
			out << YAML::Key << "CastsShadows" << YAML::Value << spotLightComponent.DoesCastShadows();
			out << YAML::Key << "VisualizeDistance" << YAML::Value << spotLightComponent.VisualizeDistanceEnabled();
			out << YAML::Key << "IsVolumetric" << YAML::Value << spotLightComponent.IsVolumetricLight();

			out << YAML::EndMap; //SpotLightComponent
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

			out << YAML::Key << "BodyType" << YAML::Value << Serializer::GetEnumName(rigidBodyComponent.BodyType);
			out << YAML::Key << "CollisionDetectionType" << YAML::Value << Serializer::GetEnumName(rigidBodyComponent.CollisionDetection);
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
			out << YAML::Key << "RollOff" << YAML::Value << Serializer::GetEnumName(audio.GetRollOffModel());
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

		if (entity.HasComponent<TextComponent>())
		{
			auto& text = entity.GetComponent<TextComponent>();

			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; //TextComponent

			SerializeRelativeTransform(out, text.GetRelativeTransform());
			Serializer::SerializeFont(out, text.GetFont());

			out << YAML::Key << "Text" << text.GetText();
			out << YAML::Key << "Color" << text.GetColor();
			out << YAML::Key << "BlendMode" << Serializer::GetEnumName(text.GetBlendMode());
			out << YAML::Key << "AlbedoColor" << text.GetAlbedoColor();
			out << YAML::Key << "EmissiveColor" << text.GetEmissiveColor();
			out << YAML::Key << "IsLit" << text.IsLit();
			out << YAML::Key << "bCastsShadows" << text.DoesCastShadows();
			out << YAML::Key << "Metallness" << text.GetMetallness();
			out << YAML::Key << "Roughness" << text.GetRoughness();
			out << YAML::Key << "AO" << text.GetAO();
			out << YAML::Key << "Opacity" << text.GetOpacity();
			out << YAML::Key << "LineSpacing" << text.GetLineSpacing();
			out << YAML::Key << "Kerning" << text.GetKerning();
			out << YAML::Key << "MaxWidth" << text.GetMaxWidth();

			out << YAML::EndMap; //TextComponent
		}
		
		if (entity.HasComponent<Text2DComponent>())
		{
			auto& text = entity.GetComponent<Text2DComponent>();

			out << YAML::Key << "Text2DComponent";
			out << YAML::BeginMap; //Text2DComponent


			Serializer::SerializeFont(out, text.GetFont());
			out << YAML::Key << "Text" << text.GetText();
			out << YAML::Key << "Color" << text.GetColor();
			out << YAML::Key << "LineSpacing" << text.GetLineSpacing();
			out << YAML::Key << "Pos" << text.GetPosition();
			out << YAML::Key << "Scale" << text.GetScale();
			out << YAML::Key << "Rotation" << text.GetRotation();
			out << YAML::Key << "Kerning" << text.GetKerning();
			out << YAML::Key << "MaxWidth" << text.GetMaxWidth();
			out << YAML::Key << "Opacity" << text.GetOpacity();

			out << YAML::EndMap; //Text2DComponent
		}

		out << YAML::EndMap; //Entity
	}

	void SceneSerializer::SerializeSkybox(YAML::Emitter& out)
	{
		const auto& sceneRenderer = m_Scene->GetSceneRenderer();
		out << YAML::Key << "Skybox" << YAML::BeginMap;
		{
			{
				out << YAML::Key << "IBL" << YAML::BeginMap;
				if (const Ref<TextureCube>& ibl = sceneRenderer->GetSkybox())
				{
					Path currentPath = std::filesystem::current_path();
					Path texturePath = std::filesystem::relative(ibl->GetPath(), currentPath);

					out << YAML::Key << "Path" << YAML::Value << texturePath.string();
					out << YAML::Key << "Size" << YAML::Value << ibl->GetSize().x;
				}
				out << YAML::EndMap;
			}

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
		out << YAML::EndMap;
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
			camera.SetProjectionMode(Serializer::GetEnumFromName<CameraProjectionMode>(cameraNode["ProjectionMode"].as<std::string>()));
			
			camera.SetPerspectiveVerticalFOV(cameraNode["PerspectiveVerticalFOV"].as<float>());
			camera.SetPerspectiveNearClip(cameraNode["PerspectiveNearClip"].as<float>());
			camera.SetPerspectiveFarClip(cameraNode["PerspectiveFarClip"].as<float>());

			camera.SetOrthographicSize(cameraNode["OrthographicSize"].as<float>());
			camera.SetOrthographicNearClip(cameraNode["OrthographicNearClip"].as<float>());
			camera.SetOrthographicFarClip(cameraNode["OrthographicFarClip"].as<float>());
			if (auto node = cameraNode["ShadowFarClip"])
				camera.SetShadowFarClip(node.as<float>());
			if (auto node = cameraNode["CascadesSplitAlpha"])
				camera.SetCascadesSplitAlpha(node.as<float>());

			DeserializeRelativeTransform(cameraComponentNode, relativeTransform);

			cameraComponent.SetRelativeTransform(relativeTransform);

			cameraComponent.Primary = cameraComponentNode["Primary"].as<bool>();
			cameraComponent.FixedAspectRatio = cameraComponentNode["FixedAspectRatio"].as<bool>();
		}

		if (auto spriteComponentNode = entityNode["SpriteComponent"])
		{
			auto& spriteComponent = deserializedEntity.AddComponent<SpriteComponent>();

			Transform relativeTransform;
			DeserializeRelativeTransform(spriteComponentNode, relativeTransform);
			spriteComponent.SetRelativeTransform(relativeTransform);

			if (auto materialNode = spriteComponentNode["Material"])
			{
				Ref<Material> material = Material::Create();
				Serializer::DeserializeMaterial(materialNode, material);
				spriteComponent.SetMaterial(material);
			}

			spriteComponent.SetIsSubTexture(spriteComponentNode["bSubTexture"].as<bool>());
			if (auto node = spriteComponentNode["bCastsShadows"])
				spriteComponent.SetCastsShadows(node.as<bool>());
			spriteComponent.SubTextureCoords = spriteComponentNode["SubTextureCoords"].as<glm::vec2>();
			spriteComponent.SpriteSize = spriteComponentNode["SpriteSize"].as<glm::vec2>();
			spriteComponent.SpriteSizeCoef = spriteComponentNode["SpriteSizeCoef"].as<glm::vec2>();
			
			Ref<Texture2D> atlas;
			Serializer::DeserializeTexture2D(spriteComponentNode, atlas, "SubTexture");
			if (atlas)
				spriteComponent.SetSubTexture(SubTexture2D::CreateFromCoords(atlas, spriteComponent.SubTextureCoords, spriteComponent.SpriteSize, spriteComponent.SpriteSizeCoef));
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
			smComponent.SetCastsShadows(staticMeshComponentNode["bCastsShadows"].as<bool>());

			Transform relativeTransform;
			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto node = staticMeshComponentNode["StaticMesh"])
			{
				Ref<StaticMesh> sm;
				Serializer::DeserializeStaticMesh(node, sm);
				smComponent.SetStaticMesh(sm);
			}
			if (auto materialNode = staticMeshComponentNode["Material"])
			{
				Ref<Material> material = Material::Create();
				Serializer::DeserializeMaterial(materialNode, material);
				smComponent.SetMaterial(material);
			}
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
			if (auto node = pointLightComponentNode["VolumetricFogIntensity"])
				pointLightComponent.SetVolumetricFogIntensity(node.as<float>());
			if (auto node = pointLightComponentNode["Radius"])
				pointLightComponent.SetRadius(node.as<float>());
			if (auto node = pointLightComponentNode["AffectsWorld"])
				pointLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = pointLightComponentNode["CastsShadows"])
				pointLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = pointLightComponentNode["VisualizeRadius"])
				pointLightComponent.SetVisualizeRadiusEnabled(node.as<bool>());
			if (auto node = pointLightComponentNode["IsVolumetric"])
				pointLightComponent.SetIsVolumetricLight(node.as<bool>());
		}

		if (auto directionalLightComponentNode = entityNode["DirectionalLightComponent"])
		{
			auto& directionalLightComponent = deserializedEntity.AddComponent<DirectionalLightComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(directionalLightComponentNode, relativeTransform);
			directionalLightComponent.SetRelativeTransform(relativeTransform);

			if (auto lightColorNode = directionalLightComponentNode["LightColor"])
				directionalLightComponent.SetLightColor(lightColorNode.as<glm::vec3>());
			if (auto ambientNode = directionalLightComponentNode["Ambient"])
				directionalLightComponent.Ambient = ambientNode.as<glm::vec3>();
			if (auto intensityNode = directionalLightComponentNode["Intensity"])
				directionalLightComponent.SetIntensity(intensityNode.as<float>());
			if (auto intensityNode = directionalLightComponentNode["VolumetricFogIntensity"])
				directionalLightComponent.SetVolumetricFogIntensity(intensityNode.as<float>());
			if (auto node = directionalLightComponentNode["AffectsWorld"])
				directionalLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = directionalLightComponentNode["CastsShadows"])
				directionalLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = directionalLightComponentNode["IsVolumetric"])
				directionalLightComponent.SetIsVolumetricLight(node.as<bool>());
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
			if (auto node = spotLightComponentNode["VolumetricFogIntensity"])
				spotLightComponent.SetVolumetricFogIntensity(node.as<float>());
			if (auto node = spotLightComponentNode["Distance"])
				spotLightComponent.SetDistance(node.as<float>());
			if (auto node = spotLightComponentNode["AffectsWorld"])
				spotLightComponent.SetAffectsWorld(node.as<bool>());
			if (auto node = spotLightComponentNode["CastsShadows"])
				spotLightComponent.SetCastsShadows(node.as<bool>());
			if (auto node = spotLightComponentNode["VisualizeDistance"])
				spotLightComponent.SetVisualizeDistanceEnabled(node.as<bool>());
			if (auto node = spotLightComponentNode["IsVolumetric"])
				spotLightComponent.SetIsVolumetricLight(node.as<bool>());
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

			rigidBodyComponent.BodyType = Serializer::GetEnumFromName<RigidBodyComponent::Type>(rigidBodyComponentNode["BodyType"].as<std::string>());
			rigidBodyComponent.CollisionDetection = Serializer::GetEnumFromName<RigidBodyComponent::CollisionDetectionType>(rigidBodyComponentNode["CollisionDetectionType"].as<std::string>());
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
			RollOffModel rollOff = Serializer::GetEnumFromName<RollOffModel>(audioNode["RollOff"].as<std::string>());
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

		if (auto textNode = entityNode["TextComponent"])
		{
			auto& text = deserializedEntity.AddComponent<TextComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(textNode, relativeTransform);
			text.SetRelativeTransform(relativeTransform);

			if (auto node = textNode["Font"])
			{
				Ref<Font> font;
				Serializer::DeserializeFont(node, font);
				text.SetFont(font);
			}
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			if (auto node = textNode["BlendMode"])
				text.SetBlendMode(Serializer::GetEnumFromName<Material::BlendMode>(node.as<std::string>()));
			text.SetAlbedoColor(textNode["AlbedoColor"].as<glm::vec3>());
			text.SetEmissiveColor(textNode["EmissiveColor"].as<glm::vec3>());
			text.SetIsLit(textNode["IsLit"].as<bool>());
			if (auto node = textNode["bCastsShadows"])
				text.SetCastsShadows(node.as<bool>());
			text.SetMetallness(textNode["Metallness"].as<float>());
			text.SetRoughness(textNode["Roughness"].as<float>());
			text.SetAO(textNode["AO"].as<float>());
			if (auto node = textNode["Opacity"])
				text.SetOpacity(node.as<float>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetKerning(textNode["Kerning"].as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
		}
		
		if (auto textNode = entityNode["Text2DComponent"])
		{
			auto& text = deserializedEntity.AddComponent<Text2DComponent>();

			if (auto node = textNode["Font"])
			{
				Ref<Font> font;
				Serializer::DeserializeFont(node, font);
				text.SetFont(font);
			}
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetPosition(textNode["Pos"].as<glm::vec2>());
			text.SetScale(textNode["Scale"].as<glm::vec2>());
			text.SetRotation(textNode["Rotation"].as<float>());
			text.SetKerning(textNode["Kerning"].as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
			text.SetOpacity(textNode["Opacity"].as<float>());
		}
	}

	void SceneSerializer::DeserializeSkybox(YAML::Node& node)
	{
		auto skybox = node["Skybox"];
		if (!skybox)
			return;

		auto& sceneRenderer = m_Scene->GetSceneRenderer();
		if (auto iblNode = skybox["IBL"])
		{
			if (auto iblImageNode = iblNode["Path"])
			{
				const Path& path = iblImageNode.as<std::string>();
				uint32_t layerSize = TextureCube::SkyboxSize;
				if (auto iblImageSize = iblNode["Size"])
					layerSize = iblImageSize.as<uint32_t>();

				Ref<Texture> texture;
				Ref<TextureCube> skybox;
				if (TextureLibrary::Get(path, &texture))
				{
					skybox = Cast<TextureCube>(texture);
					if (skybox && skybox->GetSize().x != layerSize)
						skybox.reset();
				}
				if (!skybox)
					skybox = TextureCube::Create(path, layerSize);
				sceneRenderer->SetSkybox(skybox);
			}
		}
		if (auto skyNode = skybox["Sky"])
		{
			SkySettings sky;
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
			sceneRenderer->SetSkybox(sky);
		}
		sceneRenderer->SetUseSkyAsBackground(skybox["bUseSky"].as<bool>());
	}

	void SceneSerializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		relativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
		relativeTransform.Rotation = node["RelativeRotation"].as<Rotator>();
		relativeTransform.Scale3D = node["RelativeScale"].as<glm::vec3>();
	}
}
