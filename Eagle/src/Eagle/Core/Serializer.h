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
	class AssetStaticMesh;
	class AssetSkeletalMesh;
	class AssetAudio;
	class AssetFont;
	class AssetMaterial;
	class AssetPhysicsMaterial;
	class AssetSoundGroup;
	class AssetEntity;
	class AssetAnimation;
	enum class AssetType;
	struct SceneRendererSettings;
	struct BoneNode;
	struct SkeletalMeshAnimation;

	class Serializer
	{
	public:
		static void EmitBoneNode(YAML::Emitter& out, const BoneNode& node);
		static void SerializeReverb(YAML::Emitter& out, const Ref<Reverb3D>& reverb);
		static void SerializeEntity(YAML::Emitter& out, Entity entity);
		static void SerializeRelativeTransform(YAML::Emitter& out, const Transform& relativeTransform);
		static void SerializeRendererSettings(YAML::Emitter& out, const SceneRendererSettings& settings);
		static void SerializeAnimation(YAML::Emitter& out, const SkeletalMeshAnimation& anim);
		
		static void ReadBoneNode(const YAML::Node& baseNode, BoneNode& node);
		static void DeserializeReverb(YAML::Node& reverbNode, ReverbComponent& reverb);
		static void DeserializeEntity(Entity entity, const YAML::Node& entityNode); // Doesn't handle parents
		static void DeserializeRelativeTransform(YAML::Node& node, Transform& relativeTransform);
		static void DeserializeRendererSettings(YAML::Node& node, SceneRendererSettings& settings);
		static void DeserializeAnimation(const YAML::Node& node, SkeletalMeshAnimation& animation);

		static void SerializeAsset(YAML::Emitter& out, const Ref<Asset>& asset);
		static void SerializeAssetTexture2D(YAML::Emitter& out, const Ref<AssetTexture2D>& asset);
		static void SerializeAssetTextureCube(YAML::Emitter& out, const Ref<AssetTextureCube>& asset);
		static void SerializeAssetStaticMesh(YAML::Emitter& out, const Ref<AssetStaticMesh>& asset);
		static void SerializeAssetSkeletalMesh(YAML::Emitter& out, const Ref<AssetSkeletalMesh>& asset);
		static void SerializeAssetAudio(YAML::Emitter& out, const Ref<AssetAudio>& asset);
		static void SerializeAssetFont(YAML::Emitter& out, const Ref<AssetFont>& asset);
		static void SerializeAssetMaterial(YAML::Emitter& out, const Ref<AssetMaterial>& asset);
		static void SerializeAssetPhysicsMaterial(YAML::Emitter& out, const Ref<AssetPhysicsMaterial>& asset);
		static void SerializeAssetSoundGroup(YAML::Emitter& out, const Ref<AssetSoundGroup>& asset);
		static void SerializeAssetEntity(YAML::Emitter& out, const Ref<AssetEntity>& asset);
		static void SerializeAssetAnimation(YAML::Emitter& out, const Ref<AssetAnimation>& asset);

		static Ref<Asset> DeserializeAsset(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTexture2D> DeserializeAssetTexture2D(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetTextureCube> DeserializeAssetTextureCube(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetStaticMesh> DeserializeAssetStaticMesh(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetSkeletalMesh> DeserializeAssetSkeletalMesh(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetAudio> DeserializeAssetAudio(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetFont> DeserializeAssetFont(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);
		static Ref<AssetMaterial> DeserializeAssetMaterial(const YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetPhysicsMaterial> DeserializeAssetPhysicsMaterial(const YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetSoundGroup> DeserializeAssetSoundGroup(const YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetEntity> DeserializeAssetEntity(const YAML::Node& baseNode, const Path& pathToAsset);
		static Ref<AssetAnimation> DeserializeAssetAnimation(const YAML::Node& baseNode, const Path& pathToAsset, bool bReloadRaw = false);

		static AssetType GetAssetType(const Path& pathToAsset);
		
		static void SerializePublicFieldValue(YAML::Emitter& out, const PublicField& field);
		static void DeserializePublicFieldValues(YAML::Node& publicFieldsNode, ScriptComponent& scriptComponent);
		static bool HasSerializableType(const PublicField& field);
	};
}
