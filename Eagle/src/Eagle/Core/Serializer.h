#pragma once

#include "Eagle/Core/Entity.h"
#include "Eagle/Core/Transform.h"
#include "Eagle/Core/DataBuffer.h"
#include "Eagle/Utils/Utils.h"
#include "Eagle/Utils/YamlUtils.h"

namespace Eagle
{
	class Material;
	class PhysicsMaterial;
	class PublicField;
	class ScriptComponent;
	class ReverbComponent;
	class StaticMesh;
	class Sound;
	class Texture;
	class Texture2D;
	class Reverb3D;
	class Font;
	class Asset;
	class AssetTexture2D;
	class AssetTextureCube;
	class AssetMesh;
	class AssetAudio;
	class AssetFont;
	class AssetMaterial;
	class AssetPhysicsMaterial;
	class AssetSoundGroup;
	class AssetEntity;
	enum class AssetType;

	class Serializer
	{
	public:
		static void SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb);
		static void DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb);
		static void SerializeEntity(YAML::Emitter& out, Entity entity);
		static void DeserializeEntity(Entity entity, YAML::Node& entityNode); // Doesn't handle parents
		static void SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform);
		static void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);

		static void SerializeAsset(YAML::Emitter& out, const Ref<Asset>& asset);
		static void SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset);
		static void SerializeAssetTextureCube(YAML::Emitter& out, const Ref<AssetTextureCube>& asset);
		static void SerializeAssetMesh(YAML::Emitter& out, const Ref<AssetMesh>& asset);
		static void SerializeAssetAudio(YAML::Emitter& out, const Ref<AssetAudio>& asset);
		static void SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset);
		static void SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset);
		static void SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset);
		static void SerializeAssetSoundGroup(YAML::Emitter& out, const Ref<AssetSoundGroup>& asset);
		static void SerializeAssetEntity(YAML::Emitter& out, const Ref<AssetEntity>& asset);

		static Ref<Asset> DeserializeAsset(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTexture2D> DeserializeAssetTexture2D(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTextureCube> DeserializeAssetTextureCube(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMesh> DeserializeAssetMesh(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetAudio> DeserializeAssetAudio(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetFont> DeserializeAssetFont(YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMaterial> DeserializeAssetMaterial(YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetPhysicsMaterial> DeserializeAssetPhysicsMaterial(YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetSoundGroup> DeserializeAssetSoundGroup(YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetEntity> DeserializeAssetEntity(YAML::Node& baseNode, const Path& pathToAsset);

		static AssetType GetAssetType(const Path& pathToAsset);
		
		static void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		static void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent);
		static bool HasSerializableType(const PublicField& field);
	};
}
