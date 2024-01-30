#include "egpch.h"
#include "Serializer.h"

#include "Eagle/Audio/Sound2D.h"
#include "Eagle/Audio/SoundGroup.h"
#include "Eagle/Renderer/VidWrappers/Texture.h"
#include "Eagle/Asset/AssetManager.h"
#include "Eagle/Components/Components.h"
#include "Eagle/Classes/Font.h"
#include "Eagle/Utils/PlatformUtils.h"
#include "Eagle/Utils/Compressor.h"

#include <stb_image.h>

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

	static bool SanitaryAssetChecks(const YAML::Node& baseNode, const Path& path, AssetType expectedType)
	{
		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to deserialize an asset: {}", path.u8string());
			return false;
		}

		AssetType actualType = AssetType::None;
		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		if (actualType != expectedType)
		{
			EG_CORE_ERROR("Failed to load an asset. It's not a {}: {}", Utils::GetEnumName(actualType), path.u8string());
			return false;
		}

		return true;
	}

	void Serializer::SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb)
	{
		if (reverb)
		{
			out << YAML::Key << "Reverb";
			out << YAML::BeginMap;
			out << YAML::Key << "MinDistance" << YAML::Value << reverb->GetMinDistance();
			out << YAML::Key << "MaxDistance" << YAML::Value << reverb->GetMaxDistance();
			out << YAML::Key << "Preset" << YAML::Value << Utils::GetEnumName(reverb->GetPreset());
			out << YAML::Key << "IsActive" << YAML::Value << reverb->IsActive();
			out << YAML::EndMap;
		}
	}

	void Serializer::SerializeAsset(YAML::Emitter& out, const Ref<Asset>& asset)
	{
		switch (asset->GetAssetType())
		{
			case AssetType::Texture2D:
				SerializeAssetTexture2D(out, Cast<AssetTexture2D>(asset));
				break;
			case AssetType::TextureCube:
				SerializeAssetTextureCube(out, Cast<AssetTextureCube>(asset));
				break;
			case AssetType::Mesh:
				SerializeAssetMesh(out, Cast<AssetMesh>(asset));
				break;
			case AssetType::Audio:
				SerializeAssetAudio(out, Cast<AssetAudio>(asset));
				break;
			case AssetType::Font:
				SerializeAssetFont(out, Cast<AssetFont>(asset));
				break;
			case AssetType::Material:
				SerializeAssetMaterial(out, Cast<AssetMaterial>(asset));
				break;
			case AssetType::PhysicsMaterial:
				SerializeAssetPhysicsMaterial(out, Cast<AssetPhysicsMaterial>(asset));
				break;
			case AssetType::SoundGroup:
				SerializeAssetSoundGroup(out, Cast<AssetSoundGroup>(asset));
				break;
			case AssetType::Entity:
				SerializeAssetEntity(out, Cast<AssetEntity>(asset));
				break;
			default: EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
		}
	}

	void Serializer::SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset)
	{
		const auto& texture = asset->GetTexture();
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Texture2D);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "FilterMode" << YAML::Value << Utils::GetEnumName(texture->GetFilterMode());
		out << YAML::Key << "AddressMode" << YAML::Value << Utils::GetEnumName(texture->GetAddressMode());
		out << YAML::Key << "Anisotropy" << YAML::Value << texture->GetAnisotropy();
		out << YAML::Key << "MipsCount" << YAML::Value << texture->GetMipsCount();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetTextureCube(YAML::Emitter& out, const Ref<AssetTextureCube>& asset)
	{
		const auto& textureCube = asset->GetTexture();
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::TextureCube);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Format" << YAML::Value << Utils::GetEnumName(asset->GetFormat());
		out << YAML::Key << "LayerSize" << YAML::Value << textureCube->GetSize().x;

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetMesh(YAML::Emitter& out, const Ref<AssetMesh>& asset)
	{
		const auto& mesh = asset->GetMesh();
		DataBuffer verticesBuffer{ (void*)mesh->GetVerticesData(), mesh->GetVerticesCount() * sizeof(Vertex) };
		DataBuffer indicesBuffer{ (void*)mesh->GetIndicesData(), mesh->GetIndicesCount() * sizeof(Index) };
		const size_t origVerticesDataSize = verticesBuffer.Size; // Required for decompression
		const size_t origIndicesDataSize = indicesBuffer.Size; // Required for decompression

		ScopedDataBuffer compressedVertices(Compressor::Compress(verticesBuffer));
		ScopedDataBuffer compressedIndices(Compressor::Compress(indicesBuffer));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Mesh);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Index" << YAML::Value << asset->GetMeshIndex(); // Index of a mesh if a mesh-file (fbx) contains multiple meshes

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "SizeVertices" << YAML::Value << origVerticesDataSize;
		out << YAML::Key << "SizeIndices" << YAML::Value << origIndicesDataSize;
		out << YAML::Key << "Vertices" << YAML::Value << YAML::Binary((uint8_t*)compressedVertices.Data(), compressedVertices.Size());
		out << YAML::Key << "Indices" << YAML::Value << YAML::Binary((uint8_t*)compressedIndices.Data(), compressedIndices.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetAudio(YAML::Emitter& out, const Ref<AssetAudio>& asset)
	{
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Audio);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();
		out << YAML::Key << "Volume" << YAML::Value << asset->GetAudio()->GetVolume();
		if (const auto& soundGroup = asset->GetSoundGroupAsset())
			out << YAML::Key << "SoundGroup" << YAML::Value << soundGroup->GetGUID();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset)
	{
		const ScopedDataBuffer& buffer = asset->GetRawData();
		const size_t origDataSize = buffer.Size(); // Required for decompression
		ScopedDataBuffer compressed(Compressor::Compress(DataBuffer{ (void*)buffer.Data(), buffer.Size() }));

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Font);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();
		out << YAML::Key << "RawPath" << YAML::Value << asset->GetPathToRaw().string();

		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Compressed" << YAML::Value << true;
		out << YAML::Key << "Size" << YAML::Value << origDataSize;
		out << YAML::Key << "Data" << YAML::Value << YAML::Binary((uint8_t*)compressed.Data(), compressed.Size());
		out << YAML::EndMap;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset)
	{
		const auto& material = asset->GetMaterial();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Material);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		if (const auto& textureAsset = material->GetAlbedoAsset())
			out << YAML::Key << "AlbedoTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetMetallnessAsset())
			out << YAML::Key << "MetallnessTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetNormalAsset())
			out << YAML::Key << "NormalTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetRoughnessAsset())
			out << YAML::Key << "RoughnessTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetAOAsset())
			out << YAML::Key << "AOTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetEmissiveAsset())
			out << YAML::Key << "EmissiveTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetOpacityAsset())
			out << YAML::Key << "OpacityTexture" << YAML::Value << textureAsset->GetGUID();
		if (const auto& textureAsset = material->GetOpacityMaskAsset())
			out << YAML::Key << "OpacityMaskTexture" << YAML::Value << textureAsset->GetGUID();

		out << YAML::Key << "TintColor" << YAML::Value << material->GetTintColor();
		out << YAML::Key << "EmissiveIntensity" << YAML::Value << material->GetEmissiveIntensity();
		out << YAML::Key << "TilingFactor" << YAML::Value << material->GetTilingFactor();
		out << YAML::Key << "BlendMode" << YAML::Value << Utils::GetEnumName(material->GetBlendMode());
		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset)
	{
		const auto& material = asset->GetMaterial();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::PhysicsMaterial);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		out << YAML::Key << "StaticFriction" << YAML::Value << material->StaticFriction;
		out << YAML::Key << "DynamicFriction" << YAML::Value << material->DynamicFriction;
		out << YAML::Key << "Bounciness" << YAML::Value << material->Bounciness;

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetSoundGroup(YAML::Emitter& out, const Ref<AssetSoundGroup>& asset)
	{
		const auto& soundGroup = asset->GetSoundGroup();

		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::SoundGroup);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		out << YAML::Key << "Volume" << YAML::Value << soundGroup->GetVolume();
		out << YAML::Key << "Pitch" << YAML::Value << soundGroup->GetPitch();
		out << YAML::Key << "IsPaused" << YAML::Value << soundGroup->IsPaused();
		out << YAML::Key << "IsMuted" << YAML::Value << soundGroup->IsMuted();

		out << YAML::EndMap;
	}

	void Serializer::SerializeAssetEntity(YAML::Emitter& out, const Ref<AssetEntity>& asset)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EG_VERSION;
		out << YAML::Key << "Type" << YAML::Value << Utils::GetEnumName(AssetType::Entity);
		out << YAML::Key << "GUID" << YAML::Value << asset->GetGUID();

		Serializer::SerializeEntity(out, *asset->GetEntity().get());

		out << YAML::EndMap;
	}

	void Serializer::DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb)
	{
		float minDistance = reverbNode["MinDistance"].as<float>();
		float maxDistance = reverbNode["MaxDistance"].as<float>();
		reverb.SetMinMaxDistance(minDistance, maxDistance);
		reverb.SetPreset(Utils::GetEnumFromName<ReverbPreset>(reverbNode["Preset"].as<std::string>()));
		reverb.SetActive(reverbNode["IsActive"].as<bool>());
	}

	void Serializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		if (entity.HasComponent<EntityAssetComponent>())
		{
			const auto& component = entity.GetComponent<EntityAssetComponent>();
			out << YAML::Key << "Asset" << YAML::Value << component.AssetGUID;
		}

		if (entity.HasComponent<EntitySceneNameComponent>())
		{
			const auto& sceneNameComponent = entity.GetComponent<EntitySceneNameComponent>();
			const auto& name = sceneNameComponent.Name;

			int parentID = -1;
			if (Entity parent = entity.GetParent())
				parentID = (int)parent.GetID();

			out << YAML::Key << "EntitySceneParams";
			out << YAML::BeginMap; //EntitySceneName

			out << YAML::Key << "Name" << YAML::Value << name;
			out << YAML::Key << "Parent" << YAML::Value << parentID;

			out << YAML::EndMap; //EntitySceneParams
		}

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& worldTransform = entity.GetWorldTransform();
			const auto& relativeTransform = entity.GetRelativeTransform();

			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; //TransformComponent

			out << YAML::Key << "WorldLocation" << YAML::Value << worldTransform.Location;
			out << YAML::Key << "WorldRotation" << YAML::Value << worldTransform.Rotation;
			out << YAML::Key << "WorldScale" << YAML::Value << worldTransform.Scale3D;

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

			out << YAML::Key << "ProjectionMode" << YAML::Value << Utils::GetEnumName(camera.GetProjectionMode());
			out << YAML::Key << "PerspectiveVerticalFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNearClip" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFarClip" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNearClip" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFarClip" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "ShadowFarClip" << YAML::Value << camera.GetShadowFarClip();
			out << YAML::Key << "CascadesSmoothTransitionAlpha" << YAML::Value << camera.GetCascadesSmoothTransitionAlpha();
			out << YAML::Key << "CascadesSplitAlpha" << YAML::Value << camera.GetCascadesSplitAlpha();

			out << YAML::EndMap; //Camera

			SerializeRelativeTransform(out, cameraComponent.GetRelativeTransform());

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

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

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

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

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

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

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

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

			if (const auto& asset = collider.GetPhysicsMaterialAsset())
				out << YAML::Key << "PhysicsMaterial" << YAML::Value << asset->GetGUID();

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
	}

	void Serializer::DeserializeEntity(Entity deserializedEntity, const YAML::Node& entityNode)
	{
		if (auto node = entityNode["Asset"])
		{
			if (deserializedEntity.HasComponent<EntityAssetComponent>() == false)
				deserializedEntity.AddComponent<EntityAssetComponent>();
			
			auto& component = deserializedEntity.GetComponent<EntityAssetComponent>();
			component.AssetGUID = node.as<GUID>();
		}

		if (auto sceneNameComponentNode = entityNode["EntitySceneParams"])
		{
			if (deserializedEntity.HasComponent<EntitySceneNameComponent>() == false)
				deserializedEntity.AddComponent<EntitySceneNameComponent>();
			
			auto& sceneNameComponent = deserializedEntity.GetComponent<EntitySceneNameComponent>();
			sceneNameComponent.Name = sceneNameComponentNode["Name"].as<std::string>();
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

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(boxColliderNode["PhysicsMaterial"]));
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

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(sphereColliderNode["PhysicsMaterial"]));
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

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(capsuleColliderNode["PhysicsMaterial"]));
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

			collider.SetPhysicsMaterialAsset(GetAsset<AssetPhysicsMaterial>(meshColliderNode["PhysicsMaterial"]));
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

	void Serializer::SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform)
	{
		out << YAML::Key << "RelativeLocation" << YAML::Value << relativeTransform.Location;
		out << YAML::Key << "RelativeRotation" << YAML::Value << relativeTransform.Rotation;
		out << YAML::Key << "RelativeScale" << YAML::Value << relativeTransform.Scale3D;
	}

	void Serializer::SerializeRendererSettings(YAML::Emitter& out, const SceneRendererSettings& settings)
	{
		const auto& bloomSettings = settings.BloomSettings;
		const auto& ssaoSettings = settings.SSAOSettings;
		const auto& gtaoSettings = settings.GTAOSettings;
		const auto& fogSettings = settings.FogSettings;
		const auto& volumetricSettings = settings.VolumetricSettings;
		const auto& shadowSettings = settings.ShadowsSettings;
		const auto& photoLinearParams = settings.PhotoLinearTonemappingParams;
		const auto& filmicParams = settings.FilmicTonemappingParams;

		out << YAML::Key << "RendererSettings" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "SoftShadows" << YAML::Value << settings.bEnableSoftShadows;
		out << YAML::Key << "TranslucentShadows" << YAML::Value << settings.bTranslucentShadows;
		out << YAML::Key << "ShadowsSmoothTransition" << YAML::Value << settings.bEnableCSMSmoothTransition;
		out << YAML::Key << "StutterlessShaders" << YAML::Value << settings.bStutterlessShaders;
		out << YAML::Key << "EnableObjectPicking" << YAML::Value << settings.bEnableObjectPicking;
		out << YAML::Key << "Enable2DObjectPicking" << YAML::Value << settings.bEnable2DObjectPicking;
		out << YAML::Key << "LineWidth" << YAML::Value << settings.LineWidth;
		out << YAML::Key << "GridScale" << YAML::Value << settings.GridScale;
		out << YAML::Key << "TransparencyLayers" << YAML::Value << settings.TransparencyLayers;
		out << YAML::Key << "AO" << YAML::Value << Utils::GetEnumName(settings.AO);
		out << YAML::Key << "AA" << YAML::Value << Utils::GetEnumName(settings.AA);
		out << YAML::Key << "Gamma" << YAML::Value << settings.Gamma;
		out << YAML::Key << "Exposure" << YAML::Value << settings.Exposure;
		out << YAML::Key << "TonemappingMethod" << YAML::Value << Utils::GetEnumName(settings.Tonemapping);

		out << YAML::Key << "Bloom Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Threshold" << YAML::Value << bloomSettings.Threshold;
		out << YAML::Key << "Intensity" << YAML::Value << bloomSettings.Intensity;
		out << YAML::Key << "DirtIntensity" << YAML::Value << bloomSettings.DirtIntensity;
		out << YAML::Key << "Knee" << YAML::Value << bloomSettings.Knee;
		out << YAML::Key << "bEnable" << YAML::Value << bloomSettings.bEnable;
		if (bloomSettings.Dirt)
			out << YAML::Key << "Dirt" << YAML::Value << bloomSettings.Dirt->GetGUID();
		out << YAML::EndMap; // Bloom Settings

		out << YAML::Key << "SSAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << ssaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << ssaoSettings.GetRadius();
		out << YAML::Key << "Bias" << YAML::Value << ssaoSettings.GetBias();
		out << YAML::EndMap; // SSAO Settings

		out << YAML::Key << "GTAO Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << gtaoSettings.GetNumberOfSamples();
		out << YAML::Key << "Radius" << YAML::Value << gtaoSettings.GetRadius();
		out << YAML::EndMap; // GTAO Settings

		out << YAML::Key << "Fog Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value << fogSettings.Color;
		out << YAML::Key << "MinDistance" << YAML::Value << fogSettings.MinDistance;
		out << YAML::Key << "MaxDistance" << YAML::Value << fogSettings.MaxDistance;
		out << YAML::Key << "Density" << YAML::Value << fogSettings.Density;
		out << YAML::Key << "Equation" << YAML::Value << Utils::GetEnumName(fogSettings.Equation);
		out << YAML::Key << "bEnable" << YAML::Value << fogSettings.bEnable;
		out << YAML::EndMap; // Fog Settings

		out << YAML::Key << "Volumetric Light Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Samples" << YAML::Value << volumetricSettings.Samples;
		out << YAML::Key << "MaxScatteringDistance" << YAML::Value << volumetricSettings.MaxScatteringDistance;
		out << YAML::Key << "FogSpeed" << YAML::Value << volumetricSettings.FogSpeed;
		out << YAML::Key << "bFogEnable" << YAML::Value << volumetricSettings.bFogEnable;
		out << YAML::Key << "bEnable" << YAML::Value << volumetricSettings.bEnable;
		out << YAML::EndMap; // Volumetric Light Settings

		out << YAML::Key << "Shadow Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "PointLightSize" << YAML::Value << shadowSettings.PointLightShadowMapSize;
		out << YAML::Key << "SpotLightSize" << YAML::Value << shadowSettings.SpotLightShadowMapSize;
		out << YAML::Key << "DirLightSizes" << YAML::Value << shadowSettings.DirLightShadowMapSizes;
		out << YAML::EndMap; // Shadow Settings

		out << YAML::Key << "PhotoLinear Tonemapping";
		out << YAML::BeginMap;
		out << YAML::Key << "Sensitivity" << YAML::Value << photoLinearParams.Sensitivity;
		out << YAML::Key << "ExposureTime" << YAML::Value << photoLinearParams.ExposureTime;
		out << YAML::Key << "FStop" << YAML::Value << photoLinearParams.FStop;
		out << YAML::EndMap; //PhotoLinearTonemappingSettings

		out << YAML::Key << "Filmic Tonemapping";
		out << YAML::BeginMap;
		out << YAML::Key << "WhitePoint" << YAML::Value << filmicParams.WhitePoint;
		out << YAML::EndMap; //FilmicTonemappingSettings

		out << YAML::EndMap;
	}

	void Serializer::DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform)
	{
		relativeTransform.Location = node["RelativeLocation"].as<glm::vec3>();
		relativeTransform.Rotation = node["RelativeRotation"].as<Rotator>();
		relativeTransform.Scale3D = node["RelativeScale"].as<glm::vec3>();
	}

	void Serializer::DeserializeRendererSettings(YAML::Node& node, SceneRendererSettings& settings)
	{
		auto data = node["RendererSettings"];
		if (!data)
		{
			EG_CORE_ERROR("Failed to deserialize renderer settings");
			return;
		}

		if (auto softShadows = data["SoftShadows"])
			settings.bEnableSoftShadows = softShadows.as<bool>();
		if (auto translucentShadows = data["TranslucentShadows"])
			settings.bTranslucentShadows = translucentShadows.as<bool>();
		if (auto smoothShadows = data["ShadowsSmoothTransition"])
			settings.bEnableCSMSmoothTransition = smoothShadows.as<bool>();
		if (auto stutterless = data["StutterlessShaders"])
			settings.bStutterlessShaders = stutterless.as<bool>();
		if (auto objectPicking = data["EnableObjectPicking"])
			settings.bEnableObjectPicking = objectPicking.as<bool>();
		if (auto objectPicking = data["Enable2DObjectPicking"])
			settings.bEnable2DObjectPicking = objectPicking.as<bool>();
		if (auto lineWidthNode = data["LineWidth"])
			settings.LineWidth = lineWidthNode.as<float>();
		if (auto gridScaleNode = data["GridScale"])
			settings.GridScale = gridScaleNode.as<float>();
		if (auto layersNode = data["TransparencyLayers"])
			settings.TransparencyLayers = layersNode.as<uint32_t>();
		if (auto node = data["AO"])
			settings.AO = Utils::GetEnumFromName<AmbientOcclusion>(node.as<std::string>());
		if (auto node = data["AA"])
			settings.AA = Utils::GetEnumFromName<AAMethod>(node.as<std::string>());
		if (auto gammaNode = data["Gamma"])
			settings.Gamma = gammaNode.as<float>();
		if (auto exposureNode = data["Exposure"])
			settings.Exposure = exposureNode.as<float>();
		if (auto tonemappingNode = data["TonemappingMethod"])
			settings.Tonemapping = Utils::GetEnumFromName<TonemappingMethod>(tonemappingNode.as<std::string>());

		if (auto bloomSettingsNode = data["Bloom Settings"])
		{
			settings.BloomSettings.Threshold = bloomSettingsNode["Threshold"].as<float>();
			settings.BloomSettings.Intensity = bloomSettingsNode["Intensity"].as<float>();
			settings.BloomSettings.DirtIntensity = bloomSettingsNode["DirtIntensity"].as<float>();
			settings.BloomSettings.Knee = bloomSettingsNode["Knee"].as<float>();
			settings.BloomSettings.bEnable = bloomSettingsNode["bEnable"].as<bool>();
			if (auto node = bloomSettingsNode["Dirt"])
			{
				Ref<Asset> asset;
				if (AssetManager::Get(node.as<GUID>(), &asset))
					settings.BloomSettings.Dirt = Cast<AssetTexture2D>(asset);
			}
		}

		if (auto ssaoSettingsNode = data["SSAO Settings"])
		{
			settings.SSAOSettings.SetNumberOfSamples(ssaoSettingsNode["Samples"].as<uint32_t>());
			settings.SSAOSettings.SetRadius(ssaoSettingsNode["Radius"].as<float>());
			settings.SSAOSettings.SetBias(ssaoSettingsNode["Bias"].as<float>());
		}

		if (auto gtaoSettingsNode = data["GTAO Settings"])
		{
			settings.GTAOSettings.SetNumberOfSamples(gtaoSettingsNode["Samples"].as<uint32_t>());
			settings.GTAOSettings.SetRadius(gtaoSettingsNode["Radius"].as<float>());
		}

		if (auto fogSettingsNode = data["Fog Settings"])
		{
			settings.FogSettings.Color = fogSettingsNode["Color"].as<glm::vec3>();
			settings.FogSettings.MinDistance = fogSettingsNode["MinDistance"].as<float>();
			settings.FogSettings.MaxDistance = fogSettingsNode["MaxDistance"].as<float>();
			settings.FogSettings.Density = fogSettingsNode["Density"].as<float>();
			settings.FogSettings.Equation = Utils::GetEnumFromName<FogEquation>(fogSettingsNode["Equation"].as<std::string>());
			settings.FogSettings.bEnable = fogSettingsNode["bEnable"].as<bool>();
		}

		if (auto volumetricSettingsNode = data["Volumetric Light Settings"])
		{
			settings.VolumetricSettings.Samples = volumetricSettingsNode["Samples"].as<uint32_t>();
			settings.VolumetricSettings.MaxScatteringDistance = volumetricSettingsNode["MaxScatteringDistance"].as<float>();
			if (auto node = volumetricSettingsNode["FogSpeed"])
				settings.VolumetricSettings.FogSpeed = node.as<float>();
			settings.VolumetricSettings.bFogEnable = volumetricSettingsNode["bFogEnable"].as<bool>();
			settings.VolumetricSettings.bEnable = volumetricSettingsNode["bEnable"].as<bool>();
		}

		if (auto shadowSettingsNode = data["Shadow Settings"])
		{
			settings.ShadowsSettings.PointLightShadowMapSize = shadowSettingsNode["PointLightSize"].as<uint32_t>();
			settings.ShadowsSettings.SpotLightShadowMapSize = shadowSettingsNode["SpotLightSize"].as<uint32_t>();
			settings.ShadowsSettings.DirLightShadowMapSizes = shadowSettingsNode["DirLightSizes"].as<std::vector<uint32_t>>();
		}

		if (auto photolinearNode = data["PhotoLinear Tonemapping"])
		{
			PhotoLinearTonemappingSettings params;
			params.Sensitivity = photolinearNode["Sensitivity"].as<float>();
			params.ExposureTime = photolinearNode["ExposureTime"].as<float>();
			params.FStop = photolinearNode["FStop"].as<float>();

			settings.PhotoLinearTonemappingParams = params;
		}

		if (auto filmicNode = data["Filmic Tonemapping"])
		{
			settings.FilmicTonemappingParams.WhitePoint = filmicNode["WhitePoint"].as<float>();
		}

	}

	Ref<Asset> Serializer::DeserializeAsset(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		AssetType assetType;
		auto typeNode = baseNode["Type"];
		if (!typeNode)
		{
			EG_CORE_ERROR("Failed to load an asset. It's not an eagle asset");
			return {};
		}
		assetType = Utils::GetEnumFromName<AssetType>(typeNode.as<std::string>());
		
		switch (assetType)
		{
		case AssetType::Texture2D:
			return DeserializeAssetTexture2D(baseNode, pathToAsset, bReloadRaw);
		case AssetType::TextureCube:
			return DeserializeAssetTextureCube(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Mesh:
			return DeserializeAssetMesh(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Audio:
			return DeserializeAssetAudio(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Font:
			return DeserializeAssetFont(baseNode, pathToAsset, bReloadRaw);
		case AssetType::Material:
			return DeserializeAssetMaterial(baseNode, pathToAsset);
		case AssetType::PhysicsMaterial:
			return DeserializeAssetPhysicsMaterial(baseNode, pathToAsset);
		case AssetType::SoundGroup:
			return DeserializeAssetSoundGroup(baseNode, pathToAsset);
		case AssetType::Entity:
			return DeserializeAssetEntity(baseNode, pathToAsset);
		default:
			EG_CORE_ERROR("Failed to serialize an asset. Unknown asset.");
			return {};
		}
	}

	Ref<AssetTexture2D> Serializer::DeserializeAssetTexture2D(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Texture2D))
			return {};
		
		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw.u8string());
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

		Texture2DSpecifications specs{};
		specs.FilterMode = Utils::GetEnumFromName<FilterMode>(baseNode["FilterMode"].as<std::string>());
		specs.AddressMode = Utils::GetEnumFromName<AddressMode>(baseNode["AddressMode"].as<std::string>());
		specs.MaxAnisotropy = baseNode["Anisotropy"].as<float>();
		specs.MipsCount = baseNode["MipsCount"].as<uint32_t>();

		AssetTexture2DFormat assetFormat = Utils::GetEnumFromName<AssetTexture2DFormat>(baseNode["Format"].as<std::string>());

		int width, height, channels;
		const int desiredChannels = AssetTextureFormatToChannels(assetFormat);
		void* imageData = nullptr;

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}
		imageData = stbi_load_from_memory((uint8_t*)binaryData, (int)binaryDataSize, &width, &height, &channels, desiredChannels);

		if (!imageData)
		{
			EG_CORE_ERROR("Import failed. stbi_load failed: {}", Utils::GetEnumName(assetFormat));
			return {};
		}

		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);

		class LocalAssetTexture2D : public AssetTexture2D
		{
		public:
			LocalAssetTexture2D(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Texture2D>& texture, AssetTexture2DFormat format)
				: AssetTexture2D(path, pathToRaw, guid, rawData, texture, format) {}
		};

		Ref<AssetTexture2D> asset = MakeRef<LocalAssetTexture2D>(pathToAsset, pathToRaw, guid, DataBuffer(binaryData, binaryDataSize),
			Texture2D::Create(pathToAsset.stem().u8string(), imageFormat, glm::uvec2(width, height), imageData, specs), assetFormat);

		stbi_image_free(imageData);

		return asset;
	}

	Ref<AssetTextureCube> Serializer::DeserializeAssetTextureCube(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::TextureCube))
			return {}; // Failed

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw.u8string());
			return {};
		}

		const GUID guid = baseNode["GUID"].as<GUID>();
		const AssetTextureCubeFormat assetFormat = Utils::GetEnumFromName<AssetTextureCubeFormat>(baseNode["Format"].as<std::string>());
		const uint32_t layerSize = baseNode["LayerSize"].as<uint32_t>();

		int width, height, channels;
		const int desiredChannels = AssetTextureFormatToChannels(assetFormat);

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}

		void* stbiImageData = stbi_loadf_from_memory((uint8_t*)binaryData, (int)binaryDataSize, &width, &height, &channels, desiredChannels);

		if (!stbiImageData)
		{
			EG_CORE_ERROR("Import failed. stbi_loadf failed: {} - {}", pathToAsset.u8string(), Utils::GetEnumName(assetFormat));
			return {};
		}
		const ImageFormat imageFormat = AssetTextureFormatToImageFormat(assetFormat);
		const bool bFloat16 = IsFloat16Format(assetFormat);
		void* imageData = stbiImageData;
		if (bFloat16)
		{
			const size_t pixels = size_t(width) * height * desiredChannels;
			imageData = malloc(pixels * sizeof(uint16_t));
			uint16_t* imageData16 = (uint16_t*)imageData;
			float* stbiImageData32 = (float*)stbiImageData;

			for (size_t i = 0; i < pixels; ++i)
				imageData16[i] = Utils::ToFloat16(stbiImageData32[i]);
		}

		class LocalAssetTextureCube : public AssetTextureCube
		{
		public:
			LocalAssetTextureCube(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<TextureCube>& texture, AssetTextureCubeFormat format)
				: AssetTextureCube(path, pathToRaw, guid, rawData, texture, format) {}
		};

		Ref<AssetTextureCube> asset = MakeRef<LocalAssetTextureCube>(pathToAsset, pathToRaw, guid, DataBuffer(binaryData, binaryDataSize),
			TextureCube::Create(pathToAsset.stem().u8string(), imageFormat, imageData, glm::uvec2(width, height), layerSize), assetFormat);

		if (stbiImageData != imageData)
			free(imageData);
		stbi_image_free(stbiImageData);

		return asset;
	}

	Ref<AssetMesh> Serializer::DeserializeAssetMesh(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		class LocalAssetMesh : public AssetMesh
		{
		public:
			LocalAssetMesh(const Path& path, const Path& pathToRaw, GUID guid, const Ref<StaticMesh>& mesh, uint32_t meshIndex)
				: AssetMesh(path, pathToRaw, guid, mesh, meshIndex) {}
		};

		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Mesh))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw.u8string());
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();
		const uint32_t meshIndex = baseNode["Index"].as<uint32_t>();

		if (bReloadRaw)
		{
			auto meshes = Utils::ImportMeshes(pathToRaw);
			if (meshes.size() < meshIndex)
			{
				EG_CORE_ERROR("Failed to reload a mesh asset at index {}. File {} contains {} meshes", meshIndex, pathToRaw.u8string(), meshes.size());
				return {};
			}

			return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, meshes[meshIndex], meshIndex);
		}

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinaryVertices;
		ScopedDataBuffer decompressedBinaryIndices;
		YAML::Binary yamlBinaryVertices;
		YAML::Binary yamlBinaryIndices;

		void* binaryVerticesData = nullptr;
		size_t binaryVerticesDataSize = 0ull;
		void* binaryIndicesData = nullptr;
		size_t binaryIndicesDataSize = 0ull;

		if (auto baseDataNode = baseNode["Data"])
		{
			size_t origVerticesSize = 0u;
			size_t origIndicesSize = 0u;
			bool bCompressed = false;
			if (auto node = baseDataNode["Compressed"])
			{
				bCompressed = node.as<bool>();
				if (bCompressed)
				{
					origVerticesSize = baseDataNode["SizeVertices"].as<size_t>();
					origIndicesSize = baseDataNode["SizeIndices"].as<size_t>();
				}
			}

			// Vertices
			{
				yamlBinaryVertices = baseDataNode["Vertices"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinaryVertices = Compressor::Decompress(DataBuffer{ (void*)yamlBinaryVertices.data(), yamlBinaryVertices.size() }, origVerticesSize);
					binaryVerticesData = decompressedBinaryVertices.Data();
					binaryVerticesDataSize = decompressedBinaryVertices.Size();
				}
				else
				{
					binaryVerticesData = (void*)yamlBinaryVertices.data();
					binaryVerticesDataSize = yamlBinaryVertices.size();
				}
			}

			// Indices
			{
				yamlBinaryIndices = baseDataNode["Indices"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinaryIndices = Compressor::Decompress(DataBuffer{ (void*)yamlBinaryIndices.data(), yamlBinaryIndices.size() }, origIndicesSize);
					binaryIndicesData = decompressedBinaryIndices.Data();
					binaryIndicesDataSize = decompressedBinaryIndices.Size();
				}
				else
				{
					binaryIndicesData = (void*)yamlBinaryIndices.data();
					binaryIndicesDataSize = yamlBinaryIndices.size();
				}
			}
		}

		std::vector<Vertex> vertices;
		std::vector<Index> indices;

		// Vertices
		{
			const size_t verticesCount = binaryVerticesDataSize / sizeof(Vertex);
			vertices.resize(verticesCount);
			memcpy(vertices.data(), binaryVerticesData, binaryVerticesDataSize);
		}

		// Indices
		{
			const size_t indicesCount = binaryIndicesDataSize / sizeof(Index);
			indices.resize(indicesCount);
			memcpy(indices.data(), binaryIndicesData, binaryIndicesDataSize);
		}

		return MakeRef<LocalAssetMesh>(pathToAsset, pathToRaw, guid, StaticMesh::Create(vertices, indices), meshIndex);
	}

	Ref<AssetAudio> Serializer::DeserializeAssetAudio(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Audio))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw.u8string());
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

		const float volume = baseNode["Volume"].as<float>();

		Ref<AssetSoundGroup> soundGroup = GetAsset<AssetSoundGroup>(baseNode["SoundGroup"]);

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}

		class LocalAssetAudio : public AssetAudio
		{
		public:
			LocalAssetAudio(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Audio>& audio, const Ref<AssetSoundGroup>& soundGroup)
				: AssetAudio(path, pathToRaw, guid, rawData, audio, soundGroup) {}
		};

		DataBuffer buffer = DataBuffer(binaryData, binaryDataSize);
		Ref<Audio> audio = Audio::Create(buffer);
		audio->SetVolume(volume);
		return MakeRef<LocalAssetAudio>(pathToAsset, pathToRaw, guid, buffer, audio, soundGroup);
	}

	Ref<AssetFont> Serializer::DeserializeAssetFont(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Font))
			return {};

		Path pathToRaw = baseNode["RawPath"].as<std::string>();
		if (bReloadRaw && !std::filesystem::exists(pathToRaw))
		{
			EG_CORE_ERROR("Failed to reload an asset. Raw file doesn't exist: {}", pathToRaw.u8string());
			return {};
		}

		GUID guid = baseNode["GUID"].as<GUID>();

		ScopedDataBuffer binary;
		ScopedDataBuffer decompressedBinary;
		YAML::Binary yamlBinary;

		void* binaryData = nullptr;
		size_t binaryDataSize = 0ull;
		if (bReloadRaw)
		{
			binary = FileSystem::Read(pathToRaw);
			binaryData = binary.Data();
			binaryDataSize = binary.Size();
		}
		else
		{
			if (auto baseDataNode = baseNode["Data"])
			{
				size_t origSize = 0u;
				bool bCompressed = false;
				if (auto node = baseDataNode["Compressed"])
				{
					bCompressed = node.as<bool>();
					if (bCompressed)
						origSize = baseDataNode["Size"].as<size_t>();
				}

				yamlBinary = baseDataNode["Data"].as<YAML::Binary>();
				if (bCompressed)
				{
					decompressedBinary = Compressor::Decompress(DataBuffer{ (void*)yamlBinary.data(), yamlBinary.size() }, origSize);
					binaryData = decompressedBinary.Data();
					binaryDataSize = decompressedBinary.Size();
				}
				else
				{
					binaryData = (void*)yamlBinary.data();
					binaryDataSize = yamlBinary.size();
				}
			}
		}

		class LocalAssetFont: public AssetFont
		{
		public:
			LocalAssetFont(const Path& path, const Path& pathToRaw, GUID guid, const DataBuffer& rawData, const Ref<Font>& font)
				: AssetFont(path, pathToRaw, guid, rawData, font) {}
		};

		return MakeRef<LocalAssetFont>(pathToAsset, pathToRaw, guid, DataBuffer(binaryData, binaryDataSize), Font::Create(DataBuffer(binaryData, binaryDataSize), pathToAsset.stem().u8string()));
	}

	Ref<AssetMaterial> Serializer::DeserializeAssetMaterial(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Material))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		Ref<Material> material = Material::Create();

		material->SetAlbedoAsset(GetAsset<AssetTexture2D>(baseNode["AlbedoTexture"]));
		material->SetMetallnessAsset(GetAsset<AssetTexture2D>(baseNode["MetallnessTexture"]));
		material->SetNormalAsset(GetAsset<AssetTexture2D>(baseNode["NormalTexture"]));
		material->SetRoughnessAsset(GetAsset<AssetTexture2D>(baseNode["RoughnessTexture"]));
		material->SetAOAsset(GetAsset<AssetTexture2D>(baseNode["AOTexture"]));
		material->SetEmissiveAsset(GetAsset<AssetTexture2D>(baseNode["EmissiveTexture"]));
		material->SetOpacityAsset(GetAsset<AssetTexture2D>(baseNode["OpacityTexture"]));
		material->SetOpacityMaskAsset(GetAsset<AssetTexture2D>(baseNode["OpacityMaskTexture"]));

		if (auto node = baseNode["TintColor"])
			material->SetTintColor(node.as<glm::vec4>());

		if (auto node = baseNode["EmissiveIntensity"])
			material->SetEmissiveIntensity(node.as<glm::vec3>());

		if (auto node = baseNode["TilingFactor"])
			material->SetTilingFactor(node.as<float>());

		if (auto node = baseNode["BlendMode"])
			material->SetBlendMode(Utils::GetEnumFromName<Material::BlendMode>(node.as<std::string>()));

		class LocalAssetMaterial : public AssetMaterial
		{
		public:
			LocalAssetMaterial(const Path& path, GUID guid, const Ref<Material>& material)
				: AssetMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetMaterial>(pathToAsset, guid, material);
	}

	Ref<AssetPhysicsMaterial> Serializer::DeserializeAssetPhysicsMaterial(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::PhysicsMaterial))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();

		Ref<PhysicsMaterial> material = MakeRef<PhysicsMaterial>();

		if (auto node = baseNode["StaticFriction"])
			material->StaticFriction = node.as<float>();

		if (auto node = baseNode["DynamicFriction"])
			material->DynamicFriction = node.as<float>();

		if (auto node = baseNode["Bounciness"])
			material->Bounciness = node.as<float>();

		class LocalAssetPhysicsMaterial : public AssetPhysicsMaterial
		{
		public:
			LocalAssetPhysicsMaterial(const Path& path, GUID guid, const Ref<PhysicsMaterial>& material)
				: AssetPhysicsMaterial(path, guid, material) {}
		};

		return MakeRef<LocalAssetPhysicsMaterial>(pathToAsset, guid, material);
	}

	Ref<AssetSoundGroup> Serializer::DeserializeAssetSoundGroup(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::SoundGroup))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		Ref<SoundGroup> soundGroup = SoundGroup::Create();

		const float volume = baseNode["Volume"].as<float>();
		const float pitch = baseNode["Pitch"].as<float>();
		const bool bPaused = baseNode["IsPaused"].as<bool>();
		const bool bMuted = baseNode["IsMuted"].as<bool>();
		soundGroup->SetVolume(volume);
		soundGroup->SetPitch(pitch);
		soundGroup->SetPaused(bPaused);
		soundGroup->SetMuted(bMuted);

		class LocalAssetSoundGroup : public AssetSoundGroup
		{
		public:
			LocalAssetSoundGroup(const Path& path, GUID guid, const Ref<SoundGroup>& soundGroup)
				: AssetSoundGroup(path, guid, soundGroup) {}
		};

		return MakeRef<LocalAssetSoundGroup>(pathToAsset, guid, soundGroup);
	}

	Ref<AssetEntity> Serializer::DeserializeAssetEntity(const YAML::Node& baseNode, const Path& pathToAsset)
	{
		if (!SanitaryAssetChecks(baseNode, pathToAsset, AssetType::Entity))
			return {};

		GUID guid = baseNode["GUID"].as<GUID>();
		Entity entity = AssetEntity::CreateEntity(guid);
		Serializer::DeserializeEntity(entity, baseNode);

		class LocalAssetEntity: public AssetEntity
		{
		public:
			LocalAssetEntity(const Path& path, GUID guid, const Ref<Entity>& entity)
				: AssetEntity(path, guid, entity) {}
		};

		return MakeRef<LocalAssetEntity>(pathToAsset, guid, MakeRef<Entity>(entity));
	}

	AssetType Serializer::GetAssetType(const Path& pathToAsset)
	{
		YAML::Node baseNode = YAML::LoadFile(pathToAsset.string());

		if (!baseNode)
		{
			EG_CORE_ERROR("Failed to get an asset type: {}", pathToAsset.u8string());
			return AssetType::None;
		}

		AssetType actualType = AssetType::None;
		if (auto node = baseNode["Type"])
			actualType = Utils::GetEnumFromName<AssetType>(node.as<std::string>());

		return actualType;
	}

	template<typename T>
	void SerializeField(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Value << YAML::BeginSeq << Utils::GetEnumName(field.Type) << field.GetStoredValue<T>() << YAML::EndSeq;
	}

	void Serializer::SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field)
	{
		out << YAML::Key << field.Name;
		switch (field.Type)
		{
			case FieldType::Int:
				SerializeField<int>(out, field);
				break;
			case FieldType::UnsignedInt:
				SerializeField<unsigned int>(out, field);
				break;
			case FieldType::Float:
				SerializeField<float>(out, field);
				break;
			case FieldType::String:
				SerializeField<const std::string&>(out, field);
				break;
			case FieldType::Vec2:
				SerializeField<glm::vec2>(out, field);
				break;
			case FieldType::Vec3:
			case FieldType::Color3:
				SerializeField<glm::vec3>(out, field);
				break;
			case FieldType::Vec4:
			case FieldType::Color4:
				SerializeField<glm::vec4>(out, field);
				break;
			case FieldType::Bool:
				SerializeField<bool>(out, field);
				break;
			case FieldType::Enum:
				SerializeField<int>(out, field);
				break;
			case FieldType::Asset:
			case FieldType::AssetTexture2D:
			case FieldType::AssetTextureCube:
			case FieldType::AssetMesh:
			case FieldType::AssetAudio:
			case FieldType::AssetSoundGroup:
			case FieldType::AssetFont:
			case FieldType::AssetMaterial:
			case FieldType::AssetPhysicsMaterial:
			case FieldType::AssetEntity:
			case FieldType::AssetScene:
				SerializeField<GUID>(out, field);
				break;
		}
	}

	template<typename T>
	void SetStoredValue(YAML::Node& node, PublicField& field)
	{
		T value = node.as<T>();
		field.SetStoredValue<T>(value);
	}

	void Serializer::DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent)
	{
		auto& publicFields = scriptComponent.PublicFields;
		for (auto& it : publicFieldsNode)
		{
			std::string fieldName = it.first.as<std::string>();
			FieldType fieldType = Utils::GetEnumFromName<FieldType>(it.second[0].as<std::string>());

			auto& fieldIt = publicFields.find(fieldName);
			if ((fieldIt != publicFields.end()) && (fieldType == fieldIt->second.Type))
			{
				PublicField& field = fieldIt->second;
				auto& node = it.second[1];
				switch (fieldType)
				{
					case FieldType::Int:
						SetStoredValue<int>(node, field);
						break;
					case FieldType::UnsignedInt:
						SetStoredValue<unsigned int>(node, field);
						break;
					case FieldType::Float:
						SetStoredValue<float>(node, field);
						break;
					case FieldType::String:
						SetStoredValue<std::string>(node, field);
						break;
					case FieldType::Vec2:
						SetStoredValue<glm::vec2>(node, field);
						break;
					case FieldType::Vec3:
					case FieldType::Color3:
						SetStoredValue<glm::vec3>(node, field);
						break;
					case FieldType::Vec4:
					case FieldType::Color4:
						SetStoredValue<glm::vec4>(node, field);
						break;
					case FieldType::Bool:
						SetStoredValue<bool>(node, field);
						break;
					case FieldType::Enum:
						SetStoredValue<int>(node, field);
						break;
					case FieldType::Asset:
					case FieldType::AssetTexture2D:
					case FieldType::AssetTextureCube:
					case FieldType::AssetMesh:
					case FieldType::AssetAudio:
					case FieldType::AssetSoundGroup:
					case FieldType::AssetFont:
					case FieldType::AssetMaterial:
					case FieldType::AssetPhysicsMaterial:
					case FieldType::AssetEntity:
					case FieldType::AssetScene:
						SetStoredValue<GUID>(node, field);
				}
			}
		}
	}

	bool Serializer::HasSerializableType(const PublicField& field)
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
			case FieldType::Bool: return true;
			case FieldType::Color3: return true;
			case FieldType::Color4: return true;
			case FieldType::Enum: return true;
			case FieldType::Asset:
			case FieldType::AssetTexture2D:
			case FieldType::AssetTextureCube:
			case FieldType::AssetMesh:
			case FieldType::AssetAudio:
			case FieldType::AssetSoundGroup:
			case FieldType::AssetFont:
			case FieldType::AssetMaterial:
			case FieldType::AssetPhysicsMaterial:
			case FieldType::AssetEntity:
			case FieldType::AssetScene: return true;
			default: return false;
		}
	}

}
