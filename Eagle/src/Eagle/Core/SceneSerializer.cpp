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
		out << YAML::Key << "Scene"	<< YAML::Value << "Untitled";
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;

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
		if (!data["Scene"])
		{
			EG_CORE_WARN("Can't load scene {0}. File has invalid format!", std::filesystem::absolute(filepath));
			return false;
		}
		EG_CORE_TRACE("Loading scene '{0}'", std::filesystem::absolute(filepath));

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
			if (Entity parent = entity.GetParent())
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

			out << YAML::Key << "ProjectionMode"			    << YAML::Value << Utils::GetEnumName(camera.GetProjectionMode());
			out << YAML::Key << "PerspectiveVerticalFOV"	    << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip"		    << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip"		    << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize"			    << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip"		    << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip"		    << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "ShadowFarClip"                 << YAML::Value << camera.GetShadowFarClip();
			out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
			out << YAML::Key << "CascadesSplitAlpha"            << YAML::Value << camera.GetCascadesSplitAlpha();

			out << YAML::EndMap; //Camera

			SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary"			<< YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio"	<< YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; //CameraComponent;
		}

		if (entity.HasComponent<SpriteComponent>())
		{
			const auto& spriteComponent = entity.GetComponent<SpriteComponent>();

			out << YAML::Key << "SpriteComponent";
			out << YAML::BeginMap; //SpriteComponent

			SerializeRelativeTransform(out, spriteComponent.GetRelativeTransform());

			out << YAML::Key << "bAtlas" << YAML::Value << spriteComponent.IsAtlas();
			out << YAML::Key << "bCastsShadows" << YAML::Value << spriteComponent.DoesCastShadows();
			out << YAML::Key << "AtlasSpriteCoords" << YAML::Value << spriteComponent.GetAtlasSpriteCoords();
			out << YAML::Key << "AtlasSpriteSize" << YAML::Value << spriteComponent.GetAtlasSpriteSize();
			out << YAML::Key << "AtlasSpriteSizeCoef" << YAML::Value << spriteComponent.GetAtlasSpriteSizeCoef();
			if (const auto& materialAsset = spriteComponent.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();

			out << YAML::EndMap; //SpriteComponent
		}

		if (entity.HasComponent<BillboardComponent>())
		{
			auto& component = entity.GetComponent<BillboardComponent>();

			out << YAML::Key << "BillboardComponent";
			out << YAML::BeginMap; //BillboardComponent

			SerializeRelativeTransform(out, component.GetRelativeTransform());
			if (component.TextureAsset)
				out << YAML::Key << "Texture" << YAML::Value << component.TextureAsset->GetGUID();

			out << YAML::EndMap; //BillboardComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			auto& smComponent = entity.GetComponent<StaticMeshComponent>();

			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap; //StaticMeshComponent

			out << YAML::Key << "bCastsShadows" << YAML::Value << smComponent.DoesCastShadows();

			SerializeRelativeTransform(out, smComponent.GetRelativeTransform());

			if (const auto& meshAsset = smComponent.GetMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();

			if (const auto& materialAsset = smComponent.GetMaterialAsset())
				out << YAML::Key << "Material" << YAML::Value << materialAsset->GetGUID();

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
			out << YAML::Key << "Visualize" << YAML::Value << directionalLightComponent.bVisualizeDirection;

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

			out << YAML::Key << "BodyType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.BodyType);
			out << YAML::Key << "CollisionDetectionType" << YAML::Value << Utils::GetEnumName(rigidBodyComponent.CollisionDetection);
			out << YAML::Key << "Mass" << YAML::Value << rigidBodyComponent.GetMass();
			out << YAML::Key << "LinearDamping" << YAML::Value << rigidBodyComponent.GetLinearDamping();
			out << YAML::Key << "AngularDamping" << YAML::Value << rigidBodyComponent.GetAngularDamping();
			out << YAML::Key << "MaxLinearVelocity" << YAML::Value << rigidBodyComponent.GetMaxLinearVelocity();
			out << YAML::Key << "MaxAngularVelocity" << YAML::Value << rigidBodyComponent.GetMaxAngularVelocity();
			out << YAML::Key << "EnableGravity" << YAML::Value << rigidBodyComponent.IsGravityEnabled();
			out << YAML::Key << "IsKinematic" << YAML::Value << rigidBodyComponent.IsKinematic();
			out << YAML::Key << "LockFlags" << YAML::Value << (uint32_t)rigidBodyComponent.GetLockFlags();

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

			if (const auto& meshAsset = collider.GetCollisionMeshAsset())
				out << YAML::Key << "Mesh" << YAML::Value << meshAsset->GetGUID();

			Serializer::SerializePhysicsMaterial(out, collider.GetPhysicsMaterial());

			out << YAML::Key << "IsTrigger" << YAML::Value << collider.IsTrigger();
			out << YAML::Key << "IsConvex" << YAML::Value << collider.IsConvex();
			out << YAML::Key << "IsTwoSided" << YAML::Value << collider.IsTwoSided();
			out << YAML::Key << "IsCollisionVisible" << YAML::Value << collider.IsCollisionVisible();
			out << YAML::EndMap; //MeshColliderComponent
		}

		if (entity.HasComponent<AudioComponent>())
		{
			auto& audio = entity.GetComponent<AudioComponent>();

			out << YAML::Key << "AudioComponent";
			out << YAML::BeginMap; //AudioComponent

			SerializeRelativeTransform(out, audio.GetRelativeTransform());
			if (const auto& asset = audio.GetAudioAsset())
				out << YAML::Key << "Sound" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Volume" << YAML::Value << audio.GetVolume();
			out << YAML::Key << "LoopCount" << YAML::Value << audio.GetLoopCount();
			out << YAML::Key << "IsLooping" << YAML::Value << audio.IsLooping();
			out << YAML::Key << "IsMuted" << YAML::Value << audio.IsMuted();
			out << YAML::Key << "IsStreaming" << YAML::Value << audio.IsStreaming();
			out << YAML::Key << "MinDistance" << YAML::Value << audio.GetMinDistance();
			out << YAML::Key << "MaxDistance" << YAML::Value << audio.GetMaxDistance();
			out << YAML::Key << "RollOff" << YAML::Value << Utils::GetEnumName(audio.GetRollOffModel());
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
			out << YAML::Key << "bVisualize" << YAML::Value << reverb.IsVisualizeRadiusEnabled();
			Serializer::SerializeReverb(out, reverb.GetReverb());

			out << YAML::EndMap; //ReverbComponent
		}

		if (entity.HasComponent<TextComponent>())
		{
			auto& text = entity.GetComponent<TextComponent>();

			out << YAML::Key << "TextComponent";
			out << YAML::BeginMap; //TextComponent

			SerializeRelativeTransform(out, text.GetRelativeTransform());
			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "BlendMode" << YAML::Value << Utils::GetEnumName(text.GetBlendMode());
			out << YAML::Key << "AlbedoColor" << YAML::Value << text.GetAlbedoColor();
			out << YAML::Key << "EmissiveColor" << YAML::Value << text.GetEmissiveColor();
			out << YAML::Key << "IsLit" << YAML::Value << text.IsLit();
			out << YAML::Key << "bCastsShadows" << YAML::Value << text.DoesCastShadows();
			out << YAML::Key << "Metallness" << YAML::Value << text.GetMetallness();
			out << YAML::Key << "Roughness" << YAML::Value << text.GetRoughness();
			out << YAML::Key << "AO" << YAML::Value << text.GetAO();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "Kerning" << YAML::Value << text.GetKerning();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();

			out << YAML::EndMap; //TextComponent
		}
		
		if (entity.HasComponent<Text2DComponent>())
		{
			auto& text = entity.GetComponent<Text2DComponent>();

			out << YAML::Key << "Text2DComponent";
			out << YAML::BeginMap; //Text2DComponent

			if (const auto& asset = text.GetFontAsset())
				out << YAML::Key << "Font" << YAML::Value << asset->GetGUID();

			out << YAML::Key << "Text" << YAML::Value << text.GetText();
			out << YAML::Key << "Color" << YAML::Value << text.GetColor();
			out << YAML::Key << "LineSpacing" << YAML::Value << text.GetLineSpacing();
			out << YAML::Key << "Pos" << YAML::Value << text.GetPosition();
			out << YAML::Key << "Scale" << YAML::Value << text.GetScale();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Kerning" << YAML::Value << text.GetKerning();
			out << YAML::Key << "MaxWidth" << YAML::Value << text.GetMaxWidth();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			out << YAML::EndMap; //Text2DComponent
		}

		if (entity.HasComponent<Image2DComponent>())
		{
			auto& text = entity.GetComponent<Image2DComponent>();

			out << YAML::Key << "Image2DComponent";
			out << YAML::BeginMap; //Image2DComponent

			if (const auto& asset = text.GetTextureAsset())
				out << YAML::Key << "Texture" << YAML::Value << asset->GetGUID();
			out << YAML::Key << "Tint" << YAML::Value << text.GetTint();
			out << YAML::Key << "Pos" << YAML::Value << text.GetPosition();
			out << YAML::Key << "Scale" << YAML::Value << text.GetScale();
			out << YAML::Key << "Rotation" << YAML::Value << text.GetRotation();
			out << YAML::Key << "IsVisible" << YAML::Value << text.IsVisible();
			out << YAML::Key << "Opacity" << YAML::Value << text.GetOpacity();

			out << YAML::EndMap; //Image2DComponent
		}

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
			camera.SetProjectionMode(Utils::GetEnumFromName<CameraProjectionMode>(cameraNode["ProjectionMode"].as<std::string>()));
			
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
			if (auto node = cameraNode["CascadesSmoothTransitionAlpha"])
				camera.SetCascadesSmoothTransitionAlpha(node.as<float>());

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
				spriteComponent.SetMaterialAsset(GetAsset<AssetMaterial>(materialNode));

			if (auto node = spriteComponentNode["bAtlas"])
				spriteComponent.SetIsAtlas(node.as<bool>());
			if (auto node = spriteComponentNode["bCastsShadows"])
				spriteComponent.SetCastsShadows(node.as<bool>());
			if (auto node = spriteComponentNode["AtlasSpriteCoords"])
				spriteComponent.SetAtlasSpriteCoords(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSize"])
			spriteComponent.SetAtlasSpriteSize(node.as<glm::vec2>());
			if (auto node = spriteComponentNode["AtlasSpriteSizeCoef"])
				spriteComponent.SetAtlasSpriteSizeCoef(node.as<glm::vec2>());
		}

		if (auto billboardComponentNode = entityNode["BillboardComponent"])
		{
			auto& billboardComponent = deserializedEntity.AddComponent<BillboardComponent>();
			Transform relativeTransform;

			DeserializeRelativeTransform(billboardComponentNode, relativeTransform);
			billboardComponent.TextureAsset = GetAsset<AssetTexture2D>(billboardComponentNode["Texture"]);

			billboardComponent.SetRelativeTransform(relativeTransform);
		}

		if (auto staticMeshComponentNode = entityNode["StaticMeshComponent"])
		{
			auto& smComponent = deserializedEntity.AddComponent<StaticMeshComponent>();
			smComponent.SetCastsShadows(staticMeshComponentNode["bCastsShadows"].as<bool>());

			Transform relativeTransform;
			DeserializeRelativeTransform(staticMeshComponentNode, relativeTransform);
			smComponent.SetRelativeTransform(relativeTransform);

			if (auto meshNode = staticMeshComponentNode["Mesh"])
				smComponent.SetMeshAsset(GetAsset<AssetMesh>(meshNode));
			if (auto materialNode = staticMeshComponentNode["Material"])
				smComponent.SetMaterialAsset(GetAsset<AssetMaterial>(materialNode));
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
			if (auto node = directionalLightComponentNode["Visualize"])
				directionalLightComponent.bVisualizeDirection = node.as<bool>();
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
			
			rigidBodyComponent.BodyType = Utils::GetEnumFromName<RigidBodyComponent::Type>(rigidBodyComponentNode["BodyType"].as<std::string>());
			rigidBodyComponent.CollisionDetection = Utils::GetEnumFromName<RigidBodyComponent::CollisionDetectionType>(rigidBodyComponentNode["CollisionDetectionType"].as<std::string>());
			rigidBodyComponent.SetMass(rigidBodyComponentNode["Mass"].as<float>());
			rigidBodyComponent.SetLinearDamping(rigidBodyComponentNode["LinearDamping"].as<float>());
			rigidBodyComponent.SetAngularDamping(rigidBodyComponentNode["AngularDamping"].as<float>());
			rigidBodyComponent.SetMaxLinearVelocity(rigidBodyComponentNode["MaxLinearVelocity"].as<float>());
			rigidBodyComponent.SetMaxAngularVelocity(rigidBodyComponentNode["MaxAngularVelocity"].as<float>());
			rigidBodyComponent.SetEnableGravity(rigidBodyComponentNode["EnableGravity"].as<bool>());
			rigidBodyComponent.SetIsKinematic(rigidBodyComponentNode["IsKinematic"].as<bool>());
			rigidBodyComponent.SetLockFlag(ActorLockFlag(rigidBodyComponentNode["LockFlags"].as<uint32_t>()));
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
			if (auto node = meshColliderNode["IsTwoSided"])
				collider.SetIsTwoSided(node.as<bool>());

			if (auto meshNode = meshColliderNode["Mesh"])
				collider.SetCollisionMeshAsset(GetAsset<AssetMesh>(meshNode));
		}
	
		if (auto audioNode = entityNode["AudioComponent"])
		{
			auto& audio = deserializedEntity.AddComponent<AudioComponent>();
			Path soundPath;
			
			Transform relativeTransform;
			DeserializeRelativeTransform(audioNode, relativeTransform);
			audio.SetRelativeTransform(relativeTransform);
			audio.SetAudioAsset(GetAsset<AssetAudio>(audioNode["Sound"]));

			float volume = audioNode["Volume"].as<float>();
			int loopCount = audioNode["LoopCount"].as<int>();
			bool bLooping = audioNode["IsLooping"].as<bool>();
			bool bMuted = audioNode["IsMuted"].as<bool>();
			bool bStreaming = audioNode["IsStreaming"].as<bool>();
			float minDistance = audioNode["MinDistance"].as<float>();
			float maxDistance = audioNode["MaxDistance"].as<float>();
			RollOffModel rollOff = Utils::GetEnumFromName<RollOffModel>(audioNode["RollOff"].as<std::string>());
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
		}

		if (auto reverbNode = entityNode["ReverbComponent"])
		{
			auto& reverb = deserializedEntity.AddComponent<ReverbComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(reverbNode, relativeTransform);
			reverb.SetRelativeTransform(relativeTransform);

			if (auto node = reverbNode["bVisualize"])
				reverb.SetVisualizeRadiusEnabled(node.as<bool>());

			if (auto node = reverbNode["Reverb"])
				Serializer::DeserializeReverb(node, reverb);
		}

		if (auto textNode = entityNode["TextComponent"])
		{
			auto& text = deserializedEntity.AddComponent<TextComponent>();
			
			Transform relativeTransform;
			DeserializeRelativeTransform(textNode, relativeTransform);
			text.SetRelativeTransform(relativeTransform);

			
			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			if (auto node = textNode["BlendMode"])
				text.SetBlendMode(Utils::GetEnumFromName<Material::BlendMode>(node.as<std::string>()));
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

			text.SetFontAsset(GetAsset<AssetFont>(textNode["Font"]));
			text.SetText(textNode["Text"].as<std::string>());
			text.SetColor(textNode["Color"].as<glm::vec3>());
			text.SetLineSpacing(textNode["LineSpacing"].as<float>());
			text.SetPosition(textNode["Pos"].as<glm::vec2>());
			text.SetScale(textNode["Scale"].as<glm::vec2>());
			text.SetRotation(textNode["Rotation"].as<float>());
			text.SetIsVisible(textNode["IsVisible"].as<bool>());
			text.SetKerning(textNode["Kerning"].as<float>());
			text.SetMaxWidth(textNode["MaxWidth"].as<float>());
			text.SetOpacity(textNode["Opacity"].as<float>());
		}

		if (auto imageNode = entityNode["Image2DComponent"])
		{
			auto& image2D = deserializedEntity.AddComponent<Image2DComponent>();

			Ref<AssetTexture2D> asset = GetAsset<AssetTexture2D>(imageNode["Texture"]);
			image2D.SetTextureAsset(asset);
			image2D.SetTint(imageNode["Tint"].as<glm::vec3>());
			image2D.SetPosition(imageNode["Pos"].as<glm::vec2>());
			image2D.SetScale(imageNode["Scale"].as<glm::vec2>());
			image2D.SetRotation(imageNode["Rotation"].as<float>());
			image2D.SetIsVisible(imageNode["IsVisible"].as<bool>());
			image2D.SetOpacity(imageNode["Opacity"].as<float>());
		}
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

	void SceneSerializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		relativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
		relativeTransform.Rotation = node["RelativeRotation"].as<Rotator>();
		relativeTransform.Scale3D = node["RelativeScale"].as<glm::vec3>();
	}
}
